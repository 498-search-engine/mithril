#ifndef COMMON_HTTP_REQUEST_H
#define COMMON_HTTP_REQUEST_H

#include "http/ParsedUrl.h"

#include <cstdint>
#include <string>

namespace mithril::http {

enum class Method : uint8_t { GET };

class Request {
public:
    static Request GET(std::string url);

    Method Method() const;
    const ParsedUrl& Url() const;

private:
    Request(enum Method method, ParsedUrl url);

    enum Method method_;
    ParsedUrl url_;
};

std::string BuildRawRequestString(const Request& req);

}  // namespace mithril::http

#endif
