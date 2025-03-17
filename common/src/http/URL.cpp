#include "http/URL.h"

#include "Util.h"
#include "core/array.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <spdlog/spdlog.h>

namespace mithril::http {

using namespace std::string_view_literals;

// helpers for host validation
namespace {

bool IsAlnum(char c) {
    return std::isalnum(c) != 0;
}

bool IsValidDomainLabel(std::string_view label) {
    if (label.empty() || label.size() > 63) {
        return false;
    }
    if (label.front() == '-' || label.back() == '-') {
        return false;
    }

    for (char c : label) {
        if (!IsAlnum(c) && c != '-') {
            return false;
        }
    }
    return true;
}

bool IsValidDomain(std::string_view host) {
    if (host.empty() || host.size() > MaxHostSize) {
        return false;
    }
    if (host.front() == '.' || host.back() == '.') {
        return false;
    }

    size_t start = 0;
    while (start < host.size()) {
        size_t dot = host.find('.', start);
        std::string_view label = host.substr(start, dot - start);

        if (!IsValidDomainLabel(label)) {
            return false;
        }

        start = (dot != std::string_view::npos) ? dot + 1 : host.size();
    }
    return true;
}

}  // namespace

std::optional<URL> ParseURL(std::string_view s) {
    auto u = URL{
        .url = std::string{s},
    };
    auto uv = std::string_view{u.url};
    const size_t size = uv.size();

    // Scheme validation
    size_t schemeEnd = uv.find(':');
    if (schemeEnd == std::string_view::npos || schemeEnd == 0) {
        SPDLOG_DEBUG("parse url: missing or invalid scheme in {}", uv);
        return std::nullopt;
    }

    u.scheme = uv.substr(0, schemeEnd);
    std::transform(u.scheme.begin(), u.scheme.end(), u.scheme.begin(), [](unsigned char c) { return std::tolower(c); });

    if (u.scheme != "http" && u.scheme != "https") {
        SPDLOG_DEBUG("parse url: unsupported scheme {} in {}", u.scheme, uv);
        return std::nullopt;
    }

    // Authority validation
    size_t i = schemeEnd + 1;
    size_t authorityStart = i;

    if (i + 1 < size && uv[i] == '/' && uv[i + 1] == '/') {
        i += 2;
        authorityStart = i;
    } else if (u.scheme == "http" || u.scheme == "https") {
        SPDLOG_DEBUG("parse url: missing authority component in {}", uv);
        return std::nullopt;
    }

    // Host validation
    size_t hostEnd = authorityStart;
    while (hostEnd < size) {
        if (uv[hostEnd] == '[') {
            // We just won't be doing IPv6, sorry
            return std::nullopt;
        } else if (uv[hostEnd] == ':' || uv[hostEnd] == '/' || uv[hostEnd] == '?' || uv[hostEnd] == '#') {
            break;
        }
        hostEnd++;
    }

    u.host = uv.substr(authorityStart, hostEnd - authorityStart);
    if (u.host.empty()) {
        SPDLOG_DEBUG("parse url: empty host in {}", uv);
        return std::nullopt;
    }

    if (!IsValidDomain(u.host)) {
        SPDLOG_DEBUG("parse url: invalid host {} in {}", u.host, uv);
        return std::nullopt;
    }

    // Port validation
    i = hostEnd;
    if (i < size && uv[i] == ':') {
        i++;
        size_t portStart = i;
        while (i < size && uv[i] != '/' && uv[i] != '?' && uv[i] != '#') {
            i++;
        }

        u.port = uv.substr(portStart, i - portStart);
        if (u.port.empty()) {
            SPDLOG_DEBUG("parse url: empty port in {}", uv);
            return std::nullopt;
        }

        if (!std::all_of(u.port.begin(), u.port.end(), ::isdigit)) {
            SPDLOG_DEBUG("parse url: non-numeric port {} in {}", u.port, uv);
            return std::nullopt;
        }

        try {
            const int portNum = std::stoi(u.port);
            if (portNum < 1 || portNum > 65535) {
                SPDLOG_DEBUG("parse url: port {} out of range in {}", u.port, uv);
                return std::nullopt;
            }
        } catch (const std::invalid_argument&) {
            return std::nullopt;
        } catch (const std::out_of_range&) {
            SPDLOG_DEBUG("parse url: port {} out of range in {}", u.port, uv);
            return std::nullopt;
        }
    }

    u.path = uv.substr(i);  // Rest of string

    return u;
}

std::string CanonicalizeURL(const URL& url) {
    std::string canonical;
    canonical.reserve(url.url.size());

    // Lowercase scheme and host
    std::string scheme = url.scheme;
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return std::tolower(c); });

    std::string host = url.host;
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });

    canonical += scheme + "://" + host;

    // Add non-default port
    if (!url.port.empty() && !((scheme == "http" && url.port == "80") || (scheme == "https" && url.port == "443"))) {
        canonical += ":" + url.port;
    }

    // Normalize path
    std::string cleanPath;
    cleanPath.reserve(url.path.size());
    bool prevSlash = false;

    // Ensure leading slash and collapse consecutive /'s
    if (url.path.empty() || url.path[0] != '/') {
        cleanPath += '/';
        prevSlash = true;
    }

    for (char c : url.path) {
        if (c == '/') {
            if (!prevSlash) {
                cleanPath += '/';
                prevSlash = true;
            }
            continue;
        }
        prevSlash = false;

        if (c == '#') {
            // Start of fragment, we don't want it
            break;
        }
        cleanPath += c;
    }

    // Handle trailing slash for empty paths
    if (cleanPath.empty() || (cleanPath.size() == 1 && cleanPath[0] == '/')) {
        canonical += "/";
    } else {
        canonical += cleanPath;
    }

    return canonical;
}

CanonicalHost CanonicalizeHost(const http::URL& url) {
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

constexpr core::Array<char, 16> Hex = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

std::string EncodePath(std::string_view u) {
    std::string result;
    bool inQuery = false;

    auto encodeChar = [&result](unsigned char c) {
        result += '%';
        result += Hex[c >> 4];    // First hex digit
        result += Hex[c & 0x0F];  // Second hex digit
    };

    for (unsigned char c : u) {
        bool encode = false;
        // RFC 3986 section 2.3 Unreserved Characters (allowed unencoded)
        // ALPHA / DIGIT / "-" / "." / "_" / "~"
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            encode = false;
        } else if (c == '/') {
            encode = inQuery;
        } else if (c == '?' || c == '#') {
            encode = inQuery;
            inQuery = true;
        } else if (c == '&' || c == '=') {
            encode = !inQuery;
        } else {
            encode = true;
        }

        if (encode) {
            encodeChar(c);
        } else {
            result.push_back(static_cast<char>(c));
        }
    }

    return result;
}

std::string DecodeURL(std::string_view u) {
    // RFC 3986 2.2
    constexpr std::string_view ReservedChars = ":/?#[]@!$&'()*+,;="sv;

    std::string result;
    size_t i = 0;

    while (i < u.size()) {
        if (u[i] == '%' && i < u.size() - 2) {
            unsigned char c = 0;
            auto high = u[i + 1];
            if (high >= '0' && high <= '9') {
                c = static_cast<unsigned char>((high - '0') << 4);
            } else if (high >= 'A' && high <= 'F') {
                c = static_cast<unsigned char>((high - 'A' + 10) << 4);
            } else {
                result.push_back(u[i]);
                ++i;
                continue;
            }
            auto low = u[i + 2];
            if (low >= '0' && low <= '9') {
                c |= static_cast<unsigned char>(low - '0');
            } else if (low >= 'A' && low <= 'F') {
                c |= static_cast<unsigned char>(low - 'A' + 10);
            } else {
                result.push_back(u[i]);
                ++i;
                continue;
            }

            auto decoded = static_cast<char>(c);

            if (std::find(ReservedChars.begin(), ReservedChars.end(), c) == ReservedChars.end()) {
                // Not a reserved character
                result.push_back(static_cast<char>(c));
                i += 3;
            } else {
                // Reserved character, don't decode
                result.push_back(u[i]);
                ++i;
            }
        } else {
            result.push_back(u[i]);
            ++i;
        }
    }
    return result;
}

}  // namespace mithril::http
