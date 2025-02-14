#ifndef COMMON_HTTP_REQUEST_H
#define COMMON_HTTP_REQUEST_H

#include "http/URL.h"

#include <cstdint>
#include <string>

namespace mithril::http {

enum class Method : uint8_t { GET };

struct RequestOptions {
    int followRedirects{0};
    int timeout{0};
    int maxResponseSize{0};
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
std::string BuildRawRequestString(Method method, const URL& url);

}  // namespace mithril::http

#endif
