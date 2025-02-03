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
    std::vector<char> raw;

    std::string_view headers;
    std::string_view body;
};

struct Header {
    std::string_view name;
    std::string_view value;
};

struct ParsedResponse {
    std::vector<char> raw;

    StatusCode status;
    std::vector<Header> headers;
    std::string_view body;
};

std::optional<ParsedResponse> ParseResponse(Response res);

}  // namespace mithril::http

#endif
