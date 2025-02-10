#include "http/RequestExecutor.h"

#include "html/Link.h"
#include "http/Connection.h"
#include "http/Request.h"
#include "http/Response.h"
#include "http/URL.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <unistd.h>
#include <utility>

#if defined(USE_KQUEUE)
#    include <ctime>
#endif

namespace mithril::http {

constexpr int SocketWaitTimeoutMs = 5;  // 5 milliseconds

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
    assert(events_.size() == activeConnections_.size());

    auto conn = Connection::NewFromRequest(req);
    if (!conn) {
        return;
    }

    pendingConnection_.push_back(ReqConn{
        .req = std::move(req),
        .conn = std::move(*conn),
        .state = RequestState{},
    });
}

void RequestExecutor::SetupActiveConnection(ReqConn reqConn) {
    int fd = reqConn.conn.SocketDescriptor();
    activeConnections_.emplace(fd, std::move(reqConn));
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
    ProcessPendingConnections();

    assert(events_.size() == activeConnections_.size());
    if (activeConnections_.size() == 0) {
        return;
    }

    size_t nRemoved = 0;
#if defined(USE_EPOLL)
    // TODO: untested, unchecked epoll code written by 4o
    int nev = epoll_wait(epoll_, events_.data(), events_.size(), SocketWaitTimeoutMs);
    if (nev == -1) {
        perror("RequestExecutor epoll_wait");
        exit(1);
    }

    for (int i = 0; i < nev; ++i) {
        auto& ev = events_[i];
        int fd = ev.data.fd;

        bool removed = false;
        auto connIt = activeConnections_.find(fd);
        if (connIt != activeConnections_.end()) {
            bool writingBefore = connIt->second.conn.IsWriting();

            if ((ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
                removed = HandleConnEOF(connIt);
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
    struct timespec timeout {
        .tv_sec = 0, .tv_nsec = static_cast<long>(SocketWaitTimeoutMs) * 1000L * 1000L,
    };
    int nev = kevent(kq_, nullptr, 0, events_.data(), static_cast<int>(events_.size()), &timeout);  // TODO: timeout
    for (int i = 0; i < nev; ++i) {
        auto& ev = events_[i];
        auto fd = static_cast<int>(ev.ident);

        bool removed = false;
        auto connIt = activeConnections_.find(fd);
        if (connIt != activeConnections_.end()) {
            bool writingBefore = connIt->second.conn.IsWriting();

            if ((ev.flags & EV_EOF) != 0) {
                removed = HandleConnEOF(connIt);
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

    assert(events_.size() == activeConnections_.size());
}

void RequestExecutor::ProcessPendingConnections() {
    if (pendingConnection_.empty()) {
        return;
    }

    auto it = pendingConnection_.begin();
    while (it != pendingConnection_.end()) {
        assert(it->conn.IsConnecting());
        it->conn.Connect();

        if (it->conn.IsError()) {
            // Connection failed in some way
            failedConnections_.push_back(std::move(*it));
            it = pendingConnection_.erase(it);
        } else if (it->conn.IsActive()) {
            // Now connected
            SetupActiveConnection(std::move(*it));
            it = pendingConnection_.erase(it);
        } else {
            // Still connecting, check back later
            ++it;
        }
    }
}

bool RequestExecutor::HandleConnEOF(std::unordered_map<int, ReqConn>::iterator connIt) {
    // Attempt to process any data that may still be in the socket
    connIt->second.conn.Process(true);

    if (connIt->second.conn.IsComplete()) {
        // Socket contained rest of response data after closing
        return HandleConnComplete(connIt);
    }

    // Socket has been closed without finishing response, mark as failed
    connIt->second.conn.Close();
    return HandleConnError(connIt);
}

bool RequestExecutor::HandleConnReady(std::unordered_map<int, ReqConn>::iterator connIt) {
    // Process additional received data
    connIt->second.conn.Process(false);

    // Check if the connection has reached a terminal state
    if (connIt->second.conn.IsComplete()) {
        return HandleConnComplete(connIt);
    } else if (connIt->second.conn.IsError()) {
        return HandleConnError(connIt);
    }

    // Connection is still working on sending request/getting response
    return false;
}

bool RequestExecutor::HandleConnComplete(std::unordered_map<int, ReqConn>::iterator connIt) {
    auto& conn = connIt->second.conn;
    auto& req = connIt->second.req;
    auto& state = connIt->second.state;
    assert(conn.IsComplete());

    auto res = conn.GetResponse();
    auto header = ParseResponseHeader(res);
    if (!header) {
        return HandleConnError(connIt);
    }

    if (req.Options().followRedirects > 0 && state.redirects < req.Options().followRedirects) {
        // Check for redirects
        switch (header->status) {
        case StatusCode::MovedPermanently:
        case StatusCode::Found:
        case StatusCode::TemporaryRedirect:
        case StatusCode::PermanentRedirect:
            {
                // Do redirect
                auto* loc = header->Location;
                if (loc == nullptr) {
                    // No `Location` header
                    return HandleConnError(connIt);
                }

                auto absoluteRedirect = html::MakeAbsoluteLink(conn.url_, "", loc->value);
                if (!absoluteRedirect) {
                    return HandleConnError(connIt);
                }

                auto parsedRedirect = ParseURL(*absoluteRedirect);
                if (!parsedRedirect) {
                    return HandleConnError(connIt);
                }

                std::cerr << "following redirect: " << req.Url().url << " -> " << *absoluteRedirect << std::endl;
                auto newConn = Connection::NewFromURL(req.GetMethod(), *parsedRedirect);
                if (!newConn) {
                    return HandleConnError(connIt);
                }

                // Increment number of followed redirects
                ++state.redirects;

                // Starting new connection, put into pendingConnection list
                pendingConnection_.push_back({
                    .req = std::move(req),
                    .conn = std::move(*newConn),
                    .state = state,
                });

                activeConnections_.erase(connIt);
                return true;
            }
        default:
            break;
        }
    }

    readyResponses_.push_back({
        .req = std::move(req),
        .res = std::move(res),
        .header = std::move(*header),
    });

    activeConnections_.erase(connIt);
    return true;
}

bool RequestExecutor::HandleConnError(std::unordered_map<int, ReqConn>::iterator connIt) {
    failedConnections_.push_back(std::move(connIt->second));
    activeConnections_.erase(connIt);
    return true;
}

size_t RequestExecutor::InFlightRequests() const {
    return pendingConnection_.size() + activeConnections_.size();
}

std::vector<CompleteResponse>& RequestExecutor::ReadyResponses() {
    return readyResponses_;
}

std::vector<ReqConn>& RequestExecutor::FailedConnections() {
    return failedConnections_;
}

}  // namespace mithril::http
