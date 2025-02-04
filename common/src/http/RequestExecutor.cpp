#include "http/RequestExecutor.h"

#include "http/Connection.h"
#include "http/Request.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
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
        epoll_ = -1;
    }
#elif defined(USE_KQUEUE)
    if (kq_ != -1) {
        close(kq_);
        kq_ = -1;
    }
#endif
}

void RequestExecutor::Add(Request req) {
    assert(events_.size() == connections_.size());
    auto conn = Connection::NewWithRequest(req);
    int fd = conn.SocketDescriptor();
    connections_.emplace(fd, ReqConn{.req = std::move(req), .conn = std::move(conn)});
    events_.push_back({});  // add additional space for reading this event in ProcessConnections

#if defined(USE_EPOLL)
    // Create new epoll_event and configure to notify on writes (for sending
    // request). We pass EPOLLONESHOT to only trigger the event filter once
    // after each write so we can transition to filtering on reads only once the
    // request is fully sent.
    struct epoll_event ev {};
    ev.events = EPOLLOUT | EPOLLONESHOT;
    ev.data.fd = fd;

    int status = epoll_ctl(epoll_, EPOLL_CTL_ADD, fd, &ev);
    if (status == -1) {
        perror("RequestExecutor epoll_ctl");
        exit(1);
    }
#elif defined(USE_KQUEUE)
    // Create new kevent and configure to notify on writes (for sending
    // request). We pass EV_CLEAR to clear the event filter after each write so
    // we can transition to filtering on reads only once the request is fully
    // sent.
    struct kevent ev {};
    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);

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
    assert(events_.size() == connections_.size());
    if (connections_.size() == 0) {
        return;
    }

    size_t nRemoved = 0;
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

        bool removed = false;
        auto connIt = connections_.find(fd);
        if (connIt != connections_.end()) {
            bool writingBefore = connIt->second.conn.IsWriting();

            if ((ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
                HandleConnEOF(connIt);
                removed = true;
            } else if ((ev.events & (EPOLLIN | EPOLLOUT)) != 0) {
                removed = HandleConnReady(connIt);
            }

            if (!removed) {
                if (connIt->second.conn.IsWriting()) {
                    // Still writing, only filter on writes available (resetting
                    // on every event)
                    struct epoll_event ev {};
                    ev.data.fd = fd;
                    ev.events = EPOLLOUT | EPOLLONESHOT;
                    int status = epoll_ctl(epoll_, EPOLL_CTL_MOD, fd, &ev);
                    if (status == -1) {
                        perror("RequestExecutor epoll_ctl");  // TODO: error handling strategy
                        exit(1);
                    }
                } else if (writingBefore && connIt->second.conn.IsReading()) {
                    // Transitioned from writing -> reading, only filter on
                    // reads (and don't clear on retrieving subsequent events)
                    struct epoll_event ev {};
                    ev.data.fd = fd;
                    ev.events = EPOLLIN;
                    int status = epoll_ctl(epoll_, EPOLL_CTL_MOD, fd, &ev);
                    if (status == -1) {
                        perror("RequestExecutor epoll_ctl");  // TODO: error handling strategy
                        exit(1);
                    }
                }
            }
        } else {
            // Somehow got an event for an fd we are no longer keeping track of,
            // remove it from the epoll
            struct epoll_event ev {};  // Note: ev is not actually used for EPOLL_CTL_DEL
            epoll_ctl(epoll_, EPOLL_CTL_DEL, fd, &ev);
        }

        if (removed) {
            // Note: connection is already closed, we don't need to manually
            // remove from epoll
            ++nRemoved;
        }
    }
#elif defined(USE_KQUEUE)
    int nev = kevent(kq_, nullptr, 0, events_.data(), static_cast<int>(events_.size()), nullptr);  // TODO: timeout
    for (int i = 0; i < nev; ++i) {
        auto& ev = events_[i];
        auto fd = static_cast<int>(ev.ident);

        bool removed = false;
        auto connIt = connections_.find(fd);
        if (connIt != connections_.end()) {
            bool writingBefore = connIt->second.conn.IsWriting();

            if ((ev.flags & EV_EOF) != 0) {
                HandleConnEOF(connIt);
                removed = true;
            } else if (ev.filter == EVFILT_READ || ev.filter == EVFILT_WRITE) {
                removed = HandleConnReady(connIt);
            }

            if (!removed) {
                if (connIt->second.conn.IsWriting()) {
                    // Still writing, only filter on writes available (clearing
                    // on every event)
                    struct kevent ke {};
                    EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, NULL);
                    kevent(kq_, &ke, 1, nullptr, 0, nullptr);
                } else if (writingBefore && connIt->second.conn.IsReading()) {
                    // Transitioned from writing -> reading, only filter on
                    // reads (and don't clear on retrieving subsequent events)
                    struct kevent ke {};
                    EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                    kevent(kq_, &ke, 1, nullptr, 0, nullptr);
                }
            }
        } else {
            // Somehow got an event for an fd we are no longer keeping track of,
            // remove it from the kqueue
            struct kevent ke {};
            EV_SET(&ke, fd, ev.filter, EV_DELETE, 0, 0, NULL);
            kevent(kq_, &ke, 1, nullptr, 0, nullptr);
        }

        if (removed) {
            ++nRemoved;
        }
    }
#endif

    for (size_t i = 0; i < nRemoved; ++i) {
        events_.pop_back();
    }

    assert(events_.size() == connections_.size());
}

void RequestExecutor::HandleConnEOF(std::unordered_map<int, ReqConn>::iterator connIt) {
    auto& conn = connIt->second.conn;
    auto& req = connIt->second.req;

    // Attempt to process any data that may still be in the socket
    conn.Process(true);

    if (conn.Ready()) {
        // Socket got EOF but we were able to read rest of response from socket
        readyResponses_.push_back({
            .req = std::move(req),
            .res = conn.GetResponse(),
        });
    } else {
        // Socket has been closed without finishing response, mark as failed
        conn.Close();
        failedConnections_.push_back(std::move(connIt->second));
    }

    connections_.erase(connIt);
}

bool RequestExecutor::HandleConnReady(std::unordered_map<int, ReqConn>::iterator connIt) {
    auto& conn = connIt->second.conn;
    auto& req = connIt->second.req;

    // Process additional received data
    conn.Process(false);

    if (conn.Ready()) {
        // Connection finished receiving HTTP response
        readyResponses_.push_back({
            .req = std::move(req),
            .res = conn.GetResponse(),
        });
        connections_.erase(connIt);
        return true;
    }

    return false;
}

size_t RequestExecutor::PendingConnections() const {
    return connections_.size();
}

std::vector<ReqRes>& RequestExecutor::ReadyResponses() {
    return readyResponses_;
}

std::vector<ReqConn>& RequestExecutor::FailedConnections() {
    return failedConnections_;
}

}  // namespace mithril::http
