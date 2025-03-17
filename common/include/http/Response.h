#ifndef COMMON_HTTP_RESPONSE_H
#define COMMON_HTTP_RESPONSE_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include <spdlog/spdlog.h>

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
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    TooManyRequests = 429,

    InternalServerError = 500,
};

struct Response {
    std::vector<char> header;
    std::vector<char> body;

    Response(std::vector<char> header, std::vector<char> body);

    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;

    Response(Response&&) = default;
    Response& operator=(Response&&) = default;
};

struct Header {
    std::string_view name;
    std::string_view value;
};

struct ResponseHeader {
    StatusCode status;
    std::vector<Header> headers;

    Header* ContentLanguage = nullptr;
    Header* ContentLength = nullptr;
    Header* ContentType = nullptr;
    Header* Location = nullptr;
    Header* TransferEncoding = nullptr;
};

std::optional<ResponseHeader> ParseResponseHeader(std::string_view header);

std::optional<ResponseHeader> ParseResponseHeader(const Response& res);

}  // namespace mithril::http


template<>
struct fmt::formatter<mithril::http::StatusCode> : fmt::formatter<uint16_t> {
    auto format(mithril::http::StatusCode code, format_context& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", static_cast<uint16_t>(code));
    }
};

#endif
