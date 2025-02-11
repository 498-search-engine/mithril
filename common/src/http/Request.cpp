#include "http/Request.h"

#include "http/URL.h"

namespace mithril::http {

namespace {

constexpr const char* CRLF = "\r\n";
constexpr const char* UserAgentHeader = "User-Agent: mithril-crawler/1.0 (mithril498@umich.edu)\r\n";
constexpr const char* AcceptAllHeader = "Accept: */*\r\n";
constexpr const char* AcceptEncodingHeader = "Accept-Encoding: identity\r\n";
constexpr const char* ConnectionCloseHeader = "Connection: close\r\n";

}  // namespace

Request Request::GET(URL url, RequestOptions options) {
    return Request{Method::GET, std::move(url), options};
}

Request::Request(enum Method method, URL url, RequestOptions options)
    : method_(method), url_(std::move(url)), options_(options) {}

Method Request::GetMethod() const {
    return method_;
}

const URL& Request::Url() const {
    return url_;
}

const RequestOptions& Request::Options() const {
    return options_;
}

std::string BuildRawRequestString(const Request& req) {
    return BuildRawRequestString(req.GetMethod(), req.Url());
}

std::string BuildRawRequestString(Method method, const URL& url) {
    std::string rawRequest;
    rawRequest.reserve(256);

    switch (method) {
    case Method::GET:
        rawRequest.append("GET ");
        break;
    }

    if (url.path.empty()) {
        rawRequest.append("/");
    } else {
        rawRequest.append(url.path);
    }

    rawRequest.append(" HTTP/1.1\r\nHost: ");
    rawRequest.append(url.host);
    rawRequest.append(CRLF);
    rawRequest.append(UserAgentHeader);
    rawRequest.append(AcceptAllHeader);
    rawRequest.append(AcceptEncodingHeader);
    rawRequest.append(ConnectionCloseHeader);
    rawRequest.append(CRLF);

    return rawRequest;
}

}  // namespace mithril::http
