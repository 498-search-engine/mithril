#ifndef COMMON_HTTP_REQUEST_H
#define COMMON_HTTP_REQUEST_H

#include "http/URL.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mithril::http {

enum class Method : uint8_t { GET };

struct RequestOptions {
    /**
     * @brief Max number of redirects to follow. If zero, no redirects will be
     * followed.
     */
    int followRedirects{0};

    /**
     * @brief Timeout in seconds for a response. Refreshes per-redirect. If
     * zero, no timeout is enforced.
     */
    int timeout{0};

    /**
     * @brief Max respones body size in bytes. If zero, no max response size is
     * enforced.
     */
    int maxResponseSize{0};

    /**
     * @brief Allowed Content-Type mime headers. If empty, Content-Type header
     * is not inspected.
     */
    std::vector<std::string> allowedMimeType;

    /**
     * @brief Allowed Content-Language language headers. If empty,
     * Content-Language header is not inspected.
     */
    std::vector<std::string> allowedContentLanguage;

    /**
     * @brief Whether to enable gzip compression.
     */
    bool enableCompression{false};
};

class Request {
public:
    static Request GET(URL url, RequestOptions options = {});

    Method GetMethod() const;
    const URL& Url() const;
    const RequestOptions& Options() const;

private:
    Request(Method method, URL url, RequestOptions options);

    Method method_;
    URL url_;
    RequestOptions options_;
};

std::string BuildRawRequestString(const Request& req);
std::string BuildRawRequestString(Method method, const URL& url, const RequestOptions& options);

}  // namespace mithril::http

#endif
