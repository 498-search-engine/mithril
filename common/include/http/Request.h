#ifndef COMMON_HTTP_REQUEST_H
#define COMMON_HTTP_REQUEST_H

#include "http/ParsedUrl.h"

#include <cstdint>
#include <string>

namespace mithril::http {

enum class Method : uint8_t { GET };

class Request {
public:
    static Request GET(ParsedUrl url);

    Method GetMethod() const;
    const ParsedUrl& Url() const;

private:
    Request(Method method, ParsedUrl url);

    Method method_;
    ParsedUrl url_;
};

std::string BuildRawRequestString(const Request& req);

}  // namespace mithril::http

#endif
