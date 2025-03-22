#include "http/Request.h"

#include "http/URL.h"

#include <cctype>
#include <string>
#include <utility>

namespace mithril::http {

namespace {

constexpr const char* CRLF = "\r\n";
constexpr const char* UserAgentHeader =
    "User-Agent: mithril-crawler/1.0 (mithril498@umich.edu; +https://498-search-engine.github.io/website/)\r\n";
constexpr const char* AcceptAllHeader = "Accept: */*\r\n";
constexpr const char* AcceptEncodingIdentityHeader = "Accept-Encoding: identity\r\n";
constexpr const char* AcceptEncodingGzipHeader = "Accept-Encoding: gzip\r\n";
constexpr const char* ConnectionCloseHeader = "Connection: close\r\n";

}  // namespace

Request Request::GET(URL url, RequestOptions options) {
    return Request{Method::GET, std::move(url), std::move(options)};
}

Request::Request(enum Method method, URL url, RequestOptions options)
    : method_(method), url_(std::move(url)), options_(std::move(options)) {}

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
    return BuildRawRequestString(req.GetMethod(), req.Url(), req.Options());
}

std::string BuildRawRequestString(Method method, const URL& url, const RequestOptions& options) {
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
        auto encoded = EncodePath(url.path);
        rawRequest.append(encoded);
    }

    rawRequest.append(" HTTP/1.1\r\nHost: ");
    rawRequest.append(url.host);
    rawRequest.append(CRLF);
    rawRequest.append(UserAgentHeader);
    rawRequest.append(AcceptAllHeader);
    if (options.enableCompression) {
        rawRequest.append(AcceptEncodingGzipHeader);
    } else {
        rawRequest.append(AcceptEncodingIdentityHeader);
    }
    rawRequest.append(ConnectionCloseHeader);
    rawRequest.append(CRLF);

    return rawRequest;
}

}  // namespace mithril::http
