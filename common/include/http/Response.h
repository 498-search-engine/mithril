#ifndef COMMON_HTTP_RESPONSE_H
#define COMMON_HTTP_RESPONSE_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril::http {

enum StatusCode : uint16_t {
    Continue = 100,            // RFC 9110, 15.2.1
    SwitchingProtocols = 101,  // RFC 9110, 15.2.2
    Processing = 102,          // RFC 2518, 10.1
    EarlyHints = 103,          // RFC 8297

    OK = 200,                    // RFC 9110, 15.3.1
    Created = 201,               // RFC 9110, 15.3.2
    Accepted = 202,              // RFC 9110, 15.3.3
    NonAuthoritativeInfo = 203,  // RFC 9110, 15.3.4
    NoContent = 204,             // RFC 9110, 15.3.5
    ResetContent = 205,          // RFC 9110, 15.3.6
    PartialContent = 206,        // RFC 9110, 15.3.7
    Multi = 207,                 // RFC 4918, 11.1
    AlreadyReported = 208,       // RFC 5842, 7.1
    IMUsed = 226,                // RFC 3229, 10.4.1

    MultipleChoices = 300,    // RFC 9110, 15.4.1
    MovedPermanently = 301,   // RFC 9110, 15.4.2
    Found = 302,              // RFC 9110, 15.4.3
    SeeOther = 303,           // RFC 9110, 15.4.4
    NotModified = 304,        // RFC 9110, 15.4.5
    UseProxy = 305,           // RFC 9110, 15.4.6
    TemporaryRedirect = 307,  // RFC 9110, 15.4.8
    PermanentRedirect = 308,  // RFC 9110, 15.4.9

    BadRequest = 400,                    // RFC 9110, 15.5.1
    Unauthorized = 401,                  // RFC 9110, 15.5.2
    PaymentRequired = 402,               // RFC 9110, 15.5.3
    Forbidden = 403,                     // RFC 9110, 15.5.4
    NotFound = 404,                      // RFC 9110, 15.5.5
    MethodNotAllowed = 405,              // RFC 9110, 15.5.6
    NotAcceptable = 406,                 // RFC 9110, 15.5.7
    ProxyAuthRequired = 407,             // RFC 9110, 15.5.8
    RequestTimeout = 408,                // RFC 9110, 15.5.9
    Conflict = 409,                      // RFC 9110, 15.5.10
    Gone = 410,                          // RFC 9110, 15.5.11
    LengthRequired = 411,                // RFC 9110, 15.5.12
    PreconditionFailed = 412,            // RFC 9110, 15.5.13
    RequestEntityTooLarge = 413,         // RFC 9110, 15.5.14
    RequestURITooLong = 414,             // RFC 9110, 15.5.15
    UnsupportedMediaType = 415,          // RFC 9110, 15.5.16
    RequestedRangeNotSatisfiable = 416,  // RFC 9110, 15.5.17
    ExpectationFailed = 417,             // RFC 9110, 15.5.18
    Teapot = 418,                        // RFC 9110, 15.5.19 (Unused)
    MisdirectedRequest = 421,            // RFC 9110, 15.5.20
    UnprocessableEntity = 422,           // RFC 9110, 15.5.21
    Locked = 423,                        // RFC 4918, 11.3
    FailedDependency = 424,              // RFC 4918, 11.4
    TooEarly = 425,                      // RFC 8470, 5.2.
    UpgradeRequired = 426,               // RFC 9110, 15.5.22
    PreconditionRequired = 428,          // RFC 6585, 3
    TooManyRequests = 429,               // RFC 6585, 4
    RequestHeaderFieldsTooLarge = 431,   // RFC 6585, 5
    UnavailableForLegalReasons = 451,    // RFC 7725, 3

    InternalServerError = 500,            // RFC 9110, 15.6.1
    NotImplemented = 501,                 // RFC 9110, 15.6.2
    BadGateway = 502,                     // RFC 9110, 15.6.3
    ServiceUnavailable = 503,             // RFC 9110, 15.6.4
    GatewayTimeout = 504,                 // RFC 9110, 15.6.5
    HTTPVersionNotSupported = 505,        // RFC 9110, 15.6.6
    VariantAlsoNegotiates = 506,          // RFC 2295, 8.1
    InsufficientStorage = 507,            // RFC 4918, 11.5
    LoopDetected = 508,                   // RFC 5842, 7.2
    NotExtended = 510,                    // RFC 2774, 7
    NetworkAuthenticationRequired = 511,  // RFC 6585, 6
};

constexpr std::string_view StatusText(StatusCode code) {
    using namespace std::string_view_literals;
    switch (code) {
    case OK:
        return "OK"sv;
    case Created:
        return "Created"sv;
    case Accepted:
        return "Accepted"sv;
    case NonAuthoritativeInfo:
        return "Non-Authoritative Information"sv;
    case NoContent:
        return "No Content"sv;
    case ResetContent:
        return "Reset Content"sv;
    case PartialContent:
        return "Partial Content"sv;
    case Multi:
        return "Multi-"sv;
    case AlreadyReported:
        return "Already Reported"sv;
    case IMUsed:
        return "IM Used"sv;
    case MultipleChoices:
        return "Multiple Choices"sv;
    case MovedPermanently:
        return "Moved Permanently"sv;
    case Found:
        return "Found"sv;
    case SeeOther:
        return "See Other"sv;
    case NotModified:
        return "Not Modified"sv;
    case UseProxy:
        return "Use Proxy"sv;
    case TemporaryRedirect:
        return "Temporary Redirect"sv;
    case PermanentRedirect:
        return "Permanent Redirect"sv;
    case BadRequest:
        return "Bad Request"sv;
    case Unauthorized:
        return "Unauthorized"sv;
    case PaymentRequired:
        return "Payment Required"sv;
    case Forbidden:
        return "Forbidden"sv;
    case NotFound:
        return "Not Found"sv;
    case MethodNotAllowed:
        return "Method Not Allowed"sv;
    case NotAcceptable:
        return "Not Acceptable"sv;
    case ProxyAuthRequired:
        return "Proxy Authentication Required"sv;
    case RequestTimeout:
        return "Request Timeout"sv;
    case Conflict:
        return "Conflict"sv;
    case Gone:
        return "Gone"sv;
    case LengthRequired:
        return "Length Required"sv;
    case PreconditionFailed:
        return "Precondition Failed"sv;
    case RequestEntityTooLarge:
        return "Request Entity Too Large"sv;
    case RequestURITooLong:
        return "Request URI Too Long"sv;
    case UnsupportedMediaType:
        return "Unsupported Media Type"sv;
    case RequestedRangeNotSatisfiable:
        return "Requested Range Not Satisfiable"sv;
    case ExpectationFailed:
        return "Expectation Failed"sv;
    case Teapot:
        return "I'm a teapot"sv;
    case MisdirectedRequest:
        return "Misdirected Request"sv;
    case UnprocessableEntity:
        return "Unprocessable Entity"sv;
    case Locked:
        return "Locked"sv;
    case FailedDependency:
        return "Failed Dependency"sv;
    case TooEarly:
        return "Too Early"sv;
    case UpgradeRequired:
        return "Upgrade Required"sv;
    case PreconditionRequired:
        return "Precondition Required"sv;
    case TooManyRequests:
        return "Too Many Requests"sv;
    case RequestHeaderFieldsTooLarge:
        return "Request Header Fields Too Large"sv;
    case UnavailableForLegalReasons:
        return "Unavailable For Legal Reasons"sv;
    case InternalServerError:
        return "Internal Server Error"sv;
    case NotImplemented:
        return "Not Implemented"sv;
    case BadGateway:
        return "Bad Gateway"sv;
    case ServiceUnavailable:
        return "Service Unavailable"sv;
    case GatewayTimeout:
        return "Gateway Timeout"sv;
    case HTTPVersionNotSupported:
        return "HTTP Version Not Supported"sv;
    case VariantAlsoNegotiates:
        return "Variant Also Negotiates"sv;
    case InsufficientStorage:
        return "Insufficient Storage"sv;
    case LoopDetected:
        return "Loop Detected"sv;
    case NotExtended:
        return "Not Extended"sv;
    case NetworkAuthenticationRequired:
        return "Network Authentication Required"sv;
    default:
        return ""sv;
    }
}

struct Header {
    std::string_view name;
    std::string_view value;
};

struct ResponseHeader {
    StatusCode status;
    std::vector<Header> headers;

    Header* ContentEncoding = nullptr;
    Header* ContentLanguage = nullptr;
    Header* ContentLength = nullptr;
    Header* ContentType = nullptr;
    Header* Location = nullptr;
    Header* TransferEncoding = nullptr;
};

std::optional<ResponseHeader> ParseResponseHeader(std::string_view header);

struct Response {
public:
    std::vector<char> headerData;
    std::vector<char> body;
    ResponseHeader header;

    Response(std::vector<char> header, std::vector<char> body, ResponseHeader parsedHeader);

    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;

    Response(Response&&) = default;
    Response& operator=(Response&&) = default;

    void DecodeBody();

private:
    bool decoded_;
};


bool ContentTypeMatches(std::string_view val, std::string_view mimeType);

bool ContentLanguageMatches(std::string_view val, std::string_view lang);

}  // namespace mithril::http


template<>
struct fmt::formatter<mithril::http::StatusCode> : fmt::formatter<uint16_t> {
    auto format(mithril::http::StatusCode code, format_context& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", static_cast<uint16_t>(code));
    }
};

#endif
