#ifndef COMMON_HTTP_REQUEST_H
#define COMMON_HTTP_REQUEST_H

#include "http/URL.h"

#include <cstdint>
#include <string>

namespace mithril::http {

enum class Method : uint8_t { GET };

class Request {
public:
    static Request GET(URL url);

    Method GetMethod() const;
    const URL& Url() const;

private:
    Request(Method method, URL url);

    Method method_;
    URL url_;
};

std::string BuildRawRequestString(const Request& req);

}  // namespace mithril::http

#endif
