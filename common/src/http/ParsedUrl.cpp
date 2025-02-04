#include "http/ParsedUrl.h"

#include <string>
#include <string_view>

namespace mithril::http {

ParsedUrl ParseURL(std::string url) {
    ParsedUrl u{};
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
    while (i < size && uv[i] != ':' && uv[i] != '/') {
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

}  // namespace mithril::http
