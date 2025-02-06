#include "http/Request.h"

#include "http/ParsedUrl.h"

namespace mithril::http {

namespace {

constexpr const char* CRLF = "\r\n";
constexpr const char* UserAgentHeader = "User-Agent: crawler-test/0.1\r\n";
constexpr const char* AcceptAllHeader = "Accept: */*\r\n";
constexpr const char* AcceptEncodingHeader = "Accept-Encoding: identity\r\n";
constexpr const char* ConnectionCloseHeader = "Connection: close\r\n";

}  // namespace

Request Request::GET(ParsedUrl url) {
    return Request{Method::GET, std::move(url)};
}

Request::Request(enum Method method, ParsedUrl url) : method_(method), url_(std::move(url)) {}

Method Request::GetMethod() const {
    return method_;
}

const ParsedUrl& Request::Url() const {
    return url_;
}

std::string BuildRawRequestString(const Request& req) {
    std::string rawRequest;
    rawRequest.reserve(256);

    switch (req.GetMethod()) {
    case Method::GET:
        rawRequest.append("GET ");
        break;
    }

    if (req.Url().path.empty()) {
        rawRequest.append("/");
    } else {
        rawRequest.append(req.Url().path);
    }

    rawRequest.append(" HTTP/1.1\r\nHost: ");
    rawRequest.append(req.Url().host);
    rawRequest.append(CRLF);
    rawRequest.append(UserAgentHeader);
    rawRequest.append(AcceptAllHeader);
    rawRequest.append(AcceptEncodingHeader);
    rawRequest.append(ConnectionCloseHeader);
    rawRequest.append(CRLF);

    return rawRequest;
}

}  // namespace mithril::http
