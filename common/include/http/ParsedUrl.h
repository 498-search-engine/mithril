#ifndef COMMON_HTTP_PARSEDURL_H
#define COMMON_HTTP_PARSEDURL_H

#include <cstring>
#include <string>

namespace mithril::http {

struct ParsedUrl {
    std::string url;
    std::string service;
    std::string host;
    std::string port;
    std::string path;
};

constexpr ParsedUrl ParseURL(std::string url);

}  // namespace mithril::http

#endif
