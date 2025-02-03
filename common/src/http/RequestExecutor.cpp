#include "http/RequestExecutor.h"

#include "http/Connection.h"

#include <cstddef>
#include <cstdio>
#include <unistd.h>
#include <utility>

namespace mithril::http {

RequestExecutor::RequestExecutor()
#if defined(USE_EPOLL)
    : epoll_(epoll_create1(0))
#else
    : kq_(kqueue())
#endif
{
#if defined(USE_EPOLL)
    if (epoll_ == -1) {
        perror("RequestExecutor epoll_create1");
        exit(1);
    }
    events_.reserve(10);  // Initial event buffer size
#elif defined(USE_KQUEUE)
    if (kq_ == -1) {
        perror("RequestExecutor kqueue");
        exit(1);
    }
    events_.reserve(10);  // Initial event buffer size
#endif
}

RequestExecutor::~RequestExecutor() {
#if defined(USE_EPOLL)
    if (epoll_ != -1) {
        close(epoll_);
    }
#elif defined(USE_KQUEUE)
    if (kq_ != -1) {
        close(kq_);
    }
#endif
}

void RequestExecutor::Add(Connection conn) {
    int fd = conn.SocketDescriptor();
    connections_.emplace(fd, std::move(conn));

#if defined(USE_EPOLL)
    // TODO: untested, unchecked epoll code written by 4o
    struct epoll_event ev {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    int status = epoll_ctl(epoll_, EPOLL_CTL_ADD, fd, &ev);
    if (status == -1) {
        perror("RequestExecutor epoll_ctl");
        exit(1);
    }
#elif defined(USE_KQUEUE)
    // Create new kevent and configure to notify on reads
    struct kevent ev {};
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
    events_.push_back({});  // add additional space for reading this event

    // Add kevent to kqueue
    int status = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (status == -1) {
        // TODO: error handling strategy
        perror("RequestExecutor kevent");
        exit(1);
    }
#endif
}

void RequestExecutor::ProcessConnections() {
#if defined(USE_EPOLL)
    // TODO: untested, unchecked epoll code written by 4o
    int nev = epoll_wait(epoll_, events_.data(), events_.size(), -1);
    if (nev == -1) {
        perror("RequestExecutor epoll_wait");
        exit(1);
    }

    for (int i = 0; i < nev; ++i) {
        auto& ev = events_[i];
        int fd = ev.data.fd;

        auto connIt = connections_.find(fd);
        if (connIt == connections_.end()) {
            // TODO: unclear if this can ever happen. Do we care?
            continue;
        }

        if ((ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
            auto extracted = connections_.extract(connIt);
            failedConnections_.push_back(std::move(extracted.mapped()));
        } else if ((ev.events & EPOLLIN) != 0) {
            connIt->second.Process();
            if (connIt->second.Ready()) {
                auto extracted = connections_.extract(connIt);
                readyConnections_.push_back(std::move(extracted.mapped()));
            }
        }
    }
#elif defined(USE_KQUEUE)
    int nev = kevent(kq_, nullptr, 0, events_.data(), static_cast<int>(events_.size()), nullptr);  // TODO: timeout
    for (int i = 0; i < nev; ++i) {
        auto& ev = events_[i];
        auto fd = static_cast<int>(ev.ident);

        auto connIt = connections_.find(fd);
        if (connIt == connections_.end()) {
            // TODO: unclear if this can ever happen. Do we care?
            continue;
        }

        if ((ev.flags & EV_EOF) != 0) {
            // Socket has been closed, mark as failed
            // TODO: does this always indicate failure? Or does natural closing also triggering this?
            auto extracted = connections_.extract(connIt);
            failedConnections_.push_back(std::move(extracted.mapped()));
        } else if (ev.filter == EVFILT_READ) {
            // Socket ready to be read from
            connIt->second.Process();
            if (connIt->second.Ready()) {
                // Connection finished receiving HTTP response
                auto extracted = connections_.extract(connIt);
                readyConnections_.push_back(std::move(extracted.mapped()));
            }
        }
    }
#endif
}

size_t RequestExecutor::PendingConnections() const {
    return connections_.size();
}

std::vector<Connection>& RequestExecutor::ReadyConnections() {
    return readyConnections_;
}

std::vector<Connection>& RequestExecutor::FailedConnections() {
    return failedConnections_;
}

}  // namespace mithril::http
