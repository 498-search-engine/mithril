#ifndef COMMON_HTTP_URL_H
#define COMMON_HTTP_URL_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace mithril::http {

constexpr size_t MinUrlLength = 10;
constexpr size_t MaxUrlLength = 2048;
constexpr size_t MaxHostSize = 253;

struct URL {
    std::string url;
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
};

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
 * @brief Attempts to parse a URL to an http/https resource.
 *
 * @param s URL string to parse
 * @return std::optional<URL> The parsed URL, or std::nullopt if invalid.
 */
std::optional<URL> ParseURL(std::string_view s);

/**
 * @brief Transforms a URL into its canonical form.
 */
std::string CanonicalizeURL(const URL& url);

/**
 * @brief Transforms a URL into a canonical representation of just the host
 * information (hostname, scheme, port). If a non-standard port is specified,
 * the port will be added. Otherwise, the port is stripped.
 *
 * @param url URL to canonicalize the host of
 * @return CanonicalHost
 */
CanonicalHost CanonicalizeHost(const http::URL& url);

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
