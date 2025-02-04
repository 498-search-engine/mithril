#ifndef COMMON_HTTP_REQUEST_H
#define COMMON_HTTP_REQUEST_H

#include "http/ParsedUrl.h"

#include <cstdint>
#include <string>

namespace mithril::http {

enum class Method : uint8_t { GET };

class Request {
public:
    static Request GET(std::string url, uint64_t id);

    Method Method() const;
    const ParsedUrl& Url() const;
    uint64_t Id() const;

private:
    Request(enum Method method, ParsedUrl url, uint64_t id);

    enum Method method_;
    ParsedUrl url_;
    uint64_t id_;
};

std::string BuildRawRequestString(const Request& req);

}  // namespace mithril::http

#endif
