#include "http/Request.h"

#include "http/ParsedUrl.h"

#include <cstdint>

namespace mithril::http {

Request Request::GET(std::string url, uint64_t id) {
    auto parsed = ParseURL(std::move(url));
    return Request{Method::GET, std::move(parsed), id};
}

Request::Request(enum Method method, ParsedUrl url, uint64_t id) : method_(method), url_(std::move(url)), id_(id) {}

Method Request::Method() const {
    return method_;
}

const ParsedUrl& Request::Url() const {
    return url_;
}

uint64_t Request::Id() const {
    return id_;
}

}  // namespace mithril::http
