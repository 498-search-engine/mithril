#ifndef COMMON_HTTP_URL_H
#define COMMON_HTTP_URL_H

#include "Util.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace mithril::http {

constexpr size_t MinUrlLength = 10;
constexpr size_t MaxUrlLength = 2048;

using namespace std::string_view_literals;

struct URL {
    std::string url;
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
};

// helpers for host validation
namespace {

bool IsAlnum(char c) {
    return std::isalnum(c) != 0;
}

bool IsValidDomainLabel(std::string_view label) {
    if (label.empty() || label.size() > 63)
        return false;
    if (label.front() == '-' || label.back() == '-')
        return false;

    for (char c : label) {
        if (!IsAlnum(c) && c != '-')
            return false;
    }
    return true;
}

bool IsValidDomain(std::string_view host) {
    if (host.empty() || host.size() > 253)
        return false;
    if (host.front() == '.' || host.back() == '.')
        return false;

    size_t start = 0;
    while (start < host.size()) {
        size_t dot = host.find('.', start);
        std::string_view label = host.substr(start, dot - start);

        if (!IsValidDomainLabel(label))
            return false;

        start = (dot != std::string_view::npos) ? dot + 1 : host.size();
    }
    return true;
}

}  // namespace

inline std::optional<URL> ParseURL(std::string_view url_view) {
    URL u;
    u.url = url_view;
    std::string_view uv = u.url;
    const size_t size = uv.size();

    // Scheme validation
    size_t scheme_end = uv.find(':');
    if (scheme_end == std::string_view::npos || scheme_end == 0) {
#ifndef NDEBUG
        std::cerr << "URL parse error: Missing or invalid scheme in " << uv << "\n";
#endif
        return std::nullopt;
    }

    u.scheme = uv.substr(0, scheme_end);
    std::transform(u.scheme.begin(), u.scheme.end(), u.scheme.begin(), [](unsigned char c) { return std::tolower(c); });

    if (u.scheme != "http" && u.scheme != "https") {
#ifndef NDEBUG
        std::cerr << "URL parse error: Unsupported scheme: " << u.scheme << " in " << uv << "\n";
#endif
        return std::nullopt;
    }

    // Authority validation
    size_t i = scheme_end + 1;
    size_t authority_start = i;

    if (i + 1 < size && uv[i] == '/' && uv[i + 1] == '/') {
        i += 2;
        authority_start = i;
    } else if (u.scheme == "http" || u.scheme == "https") {
#ifndef NDEBUG
        std::cerr << "URL parse error: Missing authority component in " << uv << "\n";
#endif
        return std::nullopt;
    }

    // Host validation
    size_t host_end = authority_start;
    while (host_end < size) {
        if (uv[host_end] == '[') {
            // We just won't be doing IPv6, sorry
            return std::nullopt;
        } else if (uv[host_end] == ':' || uv[host_end] == '/' || uv[host_end] == '?' || uv[host_end] == '#') {
            break;
        }
        host_end++;
    }

    u.host = uv.substr(authority_start, host_end - authority_start);
    if (u.host.empty()) {
#ifndef NDEBUG
        std::cerr << "URL parse error: Empty host in " << uv << "\n";
#endif
        return std::nullopt;
    }

    if (!IsValidDomain(u.host)) {
#ifndef NDEBUG
        std::cerr << "URL parse error: Invalid host: " << u.host << " in " << uv << "\n";
#endif
        return std::nullopt;
    }

    // Port validation
    i = host_end;
    if (i < size && uv[i] == ':') {
        i++;
        size_t port_start = i;
        while (i < size && uv[i] != '/' && uv[i] != '?' && uv[i] != '#')
            i++;

        u.port = uv.substr(port_start, i - port_start);
        if (u.port.empty()) {
#ifndef NDEBUG
            std::cerr << "URL parse error: Empty port in " << uv << "\n";
#endif
            return std::nullopt;
        }

        if (!std::all_of(u.port.begin(), u.port.end(), ::isdigit)) {
#ifndef NDEBUG
            std::cerr << "URL parse error: Non-numeric port: " << u.port << " in " << uv << "\n";
#endif
            return std::nullopt;
        }

        const int port_num = std::stoi(u.port);
        if (port_num < 1 || port_num > 65535) {
#ifndef NDEBUG
            std::cerr << "URL parse error: Port out of range: " << u.port << " in " << uv << "\n";
#endif
            return std::nullopt;
        }
    }

    u.path = uv.substr(i);  // Rest of string

    return u;
}

inline std::string CanonicalizeURL(const URL& url) {
    std::string normalized;
    normalized.reserve(url.url.size());

    // Lowercase scheme and host
    std::string scheme = url.scheme;
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return std::tolower(c); });

    std::string host = url.host;
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });

    normalized += scheme + "://" + host;

    // Add non-default port
    if (!url.port.empty() && !((scheme == "http" && url.port == "80") || (scheme == "https" && url.port == "443"))) {
        normalized += ":" + url.port;
    }

    // Normalize path
    std::string clean_path;
    clean_path.reserve(url.path.size());
    bool prev_slash = false;

    // Ensure leading slash and collapse duplicates
    if (url.path.empty() || url.path[0] != '/') {
        clean_path += '/';
        prev_slash = true;
    }

    for (char c : url.path) {
        if (c == '/') {
            if (!prev_slash) {
                clean_path += '/';
                prev_slash = true;
            }
        } else {
            clean_path += c;
            prev_slash = false;
        }
    }

    // Handle trailing slash for empty paths
    if (clean_path.empty() || (clean_path.size() == 1 && clean_path[0] == '/')) {
        normalized += "/";
    } else {
        normalized += clean_path;
    }

    return normalized;
}

// Runtime test cases (replace static_assert)
inline void TestURLParsing() {
    auto test1 = ParseURL("https://GitHub.COM/dnsge?achievement=arctic#section");
    assert(test1.has_value());
    assert(test1->scheme == "https");
    assert(test1->host == "GitHub.COM");
    assert(test1->path == "/dnsge");
    assert(CanonicalizeURL(*test1) == "https://github.com/dnsge");

    auto test2 = ParseURL("http://example.com:8080//a//b/../c");
    assert(test2.has_value());
    assert(test2->port == "8080");
    assert(test2->path == "//a//b/../c");
    assert(CanonicalizeURL(*test2) == "http://example.com:8080/a/b/../c");

    auto test3 = ParseURL("invalid://test");
    assert(!test3.has_value());
}

struct CanonicalHost {
    std::string url;
    std::string scheme;
    std::string host;
    std::string port;
};

inline bool operator==(const CanonicalHost& a, const CanonicalHost& b) {
    return a.url == b.url;
}

/**
 * @brief Transforms a URL into a canonical representation of just the host
 * information (hostname, scheme, port). If no port is specified, the implied
 * port will be added.
 *
 * @param url URL to canonicalize
 * @return CanonicalHost
 */
inline CanonicalHost CanonicalizeHost(const http::URL& url) {
    auto canonical = http::CanonicalHost{
        .url = {},
        .scheme = ToLowerCase(url.scheme),
        .host = ToLowerCase(url.host),
        .port = {},
    };

    canonical.url = canonical.scheme + "://" + canonical.host;

    if (!url.port.empty()) {
        if ((url.scheme == "https"sv && url.port != "443"sv) || (url.scheme == "http"sv && url.port != "80"sv)) {
            canonical.port = url.port;
            canonical.url += ":";
            canonical.url += canonical.port;
        }
    }

    return canonical;
}

}  // namespace mithril::http

namespace std {

template<>
struct hash<mithril::http::CanonicalHost> {
    size_t operator()(const mithril::http::CanonicalHost& c) const {
        std::hash<std::string> h;
        return h(c.url);
    }
};

}  // namespace std

#endif
