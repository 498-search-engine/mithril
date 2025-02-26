#ifndef COMMON_HTTP_REQUESTEXECUTOR_H
#define COMMON_HTTP_REQUESTEXECUTOR_H

#include "http/Connection.h"
#include "http/Request.h"
#include "http/Response.h"

#include <cstddef>
#include <list>
#include <string>
#include <string_view>
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

struct RequestState {
    int redirects{0};
    long startTime{0};
};

struct CompleteResponse {
    Request req;
    Response res;
    ResponseHeader header;
};

enum class RequestError : uint8_t {
    None,
    ConnectionError,
    InvalidResponseData,

    RedirectError,
    TooManyRedirects,
    TimedOut,
    ResponseTooBig,
};

struct FailedRequest {
    Request req;
    RequestError error{RequestError::None};
};

constexpr std::string_view StringOfRequestError(RequestError e) {
    using namespace std::string_view_literals;
    switch (e) {
    case RequestError::None:
        return "None"sv;
    case RequestError::ConnectionError:
        return "ConnectionError"sv;
    case RequestError::InvalidResponseData:
        return "InvalidResponseData"sv;
    case RequestError::RedirectError:
        return "RedirectError"sv;
    case RequestError::TooManyRedirects:
        return "TooManyRedirects"sv;
    case RequestError::TimedOut:
        return "TimedOut"sv;
    case RequestError::ResponseTooBig:
        return "ResponseTooBig"sv;
    default:
        return "Unkown"sv;
    }
}

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
     * @brief Returns the number of requests currently in-flight.
     */
    size_t InFlightRequests() const;

    /**
     * @brief Returns vector containing complete HTTP responses.
     */
    std::vector<CompleteResponse>& ReadyResponses();

    /**
     * @brief Returns vector containing HTTP requests that failed to receive a
     * response.
     */
    std::vector<FailedRequest>& FailedRequests();

    void DumpUnprocessedRequests(std::vector<std::string>& out) const;

private:
    struct ReqConn {
        Request req;
        Connection conn;
        RequestState state;
    };

    /**
     * @brief Check all requests with configured timeouts for having timed out.
     *
     * @return size_t Number of requests timed out and failed.
     */
    size_t CheckRequestTimeouts();

    bool HandleConnEOF(std::unordered_map<int, ReqConn>::iterator connIt);
    bool HandleConnReady(std::unordered_map<int, ReqConn>::iterator connIt);

    bool HandleConnComplete(std::unordered_map<int, ReqConn>::iterator connIt);
    bool HandleConnError(std::unordered_map<int, ReqConn>::iterator connIt, RequestError error);

    void ProcessPendingConnections();
    void SetupActiveConnection(ReqConn reqConn);

#if defined(USE_EPOLL)
    int epoll_;
    std::vector<struct epoll_event> events_;
#else
    int kq_;
    std::vector<struct kevent> events_;
#endif

    std::list<ReqConn> pendingConnection_;
    std::unordered_map<int, ReqConn> activeConnections_;
    std::vector<CompleteResponse> readyResponses_;
    std::vector<FailedRequest> failedRequests_;
};

}  // namespace mithril::http

#endif
