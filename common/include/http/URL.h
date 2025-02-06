#ifndef COMMON_HTTP_PARSEDURL_H
#define COMMON_HTTP_PARSEDURL_H

#include <cstring>
#include <string>

namespace mithril::http {

struct URL {
    std::string url;
    std::string service;
    std::string host;
    std::string port;
    std::string path;
};

constexpr URL ParseURL(std::string url) {
    URL u{};
    u.url = std::move(url);
    auto uv = std::string_view{u.url};
    size_t size = u.url.size();

    size_t i = 0;
    size_t start = 0;

    // Get service: i.e. http, https
    while (i < size && uv[i] != ':') {
        ++i;
    }
    if (i >= size) {
        return u;
    }

    u.service = uv.substr(start, i - start);
    ++i;  // Consume :

    // Consume the // before host
    if (i < size && uv[i] == '/') {
        ++i;
    }
    if (i < size && uv[i] == '/') {
        ++i;
    }
    if (i >= size) {
        return u;
    }

    start = i;
    // Advance until : for port or / for path
    while (i < size && uv[i] != ':' && uv[i] != '/' && uv[i] != '?') {
        ++i;
    }

    u.host = uv.substr(start, i - start);

    if (i >= size) {
        return u;
    }

    if (uv[i] == ':') {
        // Get port
        start = i + 1;
        while (i < size && uv[i] != '/') {
            ++i;
        }
        u.port = uv.substr(start, i - start);
    }

    u.path = uv.substr(i);

    return u;
}

static_assert(ParseURL("https://docs.github.com/hello/world.txt").service == "https");
static_assert(ParseURL("https://docs.github.com/hello/world.txt").port == "");
static_assert(ParseURL("https://docs.github.com/hello/world.txt").path == "/hello/world.txt");

// Yes, links like this exist
static_assert(ParseURL("https://docs.github.com?123").host == "docs.github.com");
static_assert(ParseURL("https://docs.github.com?123").path == "?123");

}  // namespace mithril::http

#endif
