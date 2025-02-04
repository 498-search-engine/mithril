#ifndef COMMON_HTTP_REQUESTEXECUTOR_H
#define COMMON_HTTP_REQUESTEXECUTOR_H

#include "http/Connection.h"
#include "http/Request.h"

#include <unordered_map>
#include <vector>

#if defined(__linux__)
#    define USE_EPOLL
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#    define USE_KQUEUE
#else
#    error Unsure whether to use epoll or kqueue
#endif

#if defined(USE_EPOLL)
#    include <sys/epoll.h>  // epoll_create1(), epoll_ctl(), epoll_wait()
#elif defined(USE_KQUEUE)
#    include <sys/event.h>  // kqueue(), kevent()
#endif

namespace mithril::http {

struct ReqConn {
    Request req;
    Connection conn;
};

struct ReqRes {
    Request req;
    Response res;
};

/**
 * @brief RequestExecutor processes many HTTP Connection instances concurrently,
 * processing them until the response has been fully read from the server.
 */
class RequestExecutor {
public:
    RequestExecutor();
    ~RequestExecutor();

    /**
     * @brief Adds a new HTTP request to execute.
     *
     * @param req Request to add.
     */
    void Add(Request req);

    /**
     * @brief Processes events from all managed connections.
     */
    void ProcessConnections();

    /**
     * @brief Returns the number of connections currently pending.
     */
    size_t PendingConnections() const;

    /**
     * @brief Returns vector containing complete HTTP responses.
     */
    std::vector<ReqRes>& ReadyResponses();

    /**
     * @brief Returns vector containing HTTP connections that failed to
     * completely receive a response.
     */
    std::vector<ReqConn>& FailedConnections();

private:
    void HandleConnEOF(std::unordered_map<int, ReqConn>::iterator connIt);
    bool HandleConnRead(std::unordered_map<int, ReqConn>::iterator connIt);

#if defined(USE_EPOLL)
    int epoll_;
    std::vector<struct epoll_event> events_;
#else
    int kq_;
    std::vector<struct kevent> events_;
#endif


    std::unordered_map<int, ReqConn> connections_;
    std::vector<ReqRes> readyResponses_;
    std::vector<ReqConn> failedConnections_;
};

}  // namespace mithril::http

#endif
