#ifndef COMMON_HTTP_RESPONSE_H
#define COMMON_HTTP_RESPONSE_H

#include <optional>
#include <vector>

namespace mithril::http {

enum StatusCode : uint16_t {
    OK = 200,

    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    BadRequest = 400,
    NotFound = 404,
    TooManyRequests = 429,

    InternalServerError = 500,
};

struct Response {
    std::vector<char> header;
    std::vector<char> body;
};

struct Header {
    std::string_view name;
    std::string_view value;
};

struct ResponseHeader {
    StatusCode status;
    std::vector<Header> headers;
};

std::optional<ResponseHeader> ParseResponseHeader(const Response& res);

}  // namespace mithril::http

#endif
