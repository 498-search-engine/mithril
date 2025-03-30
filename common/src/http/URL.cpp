#include "http/URL.h"

#include "Util.h"
#include "core/array.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril::http {

using namespace std::string_view_literals;

// helpers for host validation
namespace {

const std::set<std::string_view> DiscardURLQueryParameters = {
    // https://en.wikipedia.org/wiki/UTM_parameters#Parameters
    "utm_source"sv,
    "utm_medium"sv,
    "utm_campaign"sv,
    "utm_term"sv,
    "utm_content"sv,
    // Google analytics
    "_ga"sv,
    "_gl"sv,
    "_gac"sv,
    "gclid"sv,
    // Referral
    "ref"sv,
    "referrer"sv,
    "referer"sv,
    "source"sv,
    // Cache/timestamp/uniqueness, etc.
    "_"sv,
    "_t"sv,
    "timestamp"sv,
    "nocache"sv,
    "random"sv,
    "rand"sv,
    // Session ID
    "sid"sv,
    "session_id"sv,
    "sessionid"sv,
    "visitor_id"sv,
    "visitorid"sv,
};

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

/**
 * @brief Filters out query parameters from a URL and sorts the remaining ones.
 *
 * @param path URL path with parameters to clean
 * @param toRemove Query parameter names to remove (case sensitive)
 */
std::string CleanQueryParameters(std::string_view path, const std::set<std::string_view>& toRemove) {
    // Find position of the query part
    size_t queryPos = path.find('?');

    // If no query parameters, return the path as is
    if (queryPos == std::string::npos) {
        return std::string{path};
    }

    // Base path without query
    auto basePath = std::string{path.substr(0, queryPos)};

    // If there's nothing after the '?', return just the base path
    if (queryPos == path.size() - 1) {
        return basePath;
    }

    // Parse and filter query parameters
    auto queryPart = path.substr(queryPos + 1);
    size_t startPos = 0;
    size_t ampPos;
    std::vector<std::pair<std::string_view, std::string_view>> params;

    while (startPos < queryPart.size()) {
        ampPos = queryPart.find('&', startPos);
        if (ampPos == std::string::npos) {
            ampPos = queryPart.size();
        }

        auto param = queryPart.substr(startPos, ampPos - startPos);
        size_t equalsPos = param.find('=');

        std::string_view paramName;
        std::string_view paramValue;
        if (equalsPos != std::string::npos) {
            paramName = param.substr(0, equalsPos);
            paramValue = param.substr(equalsPos + 1);
        } else {
            paramName = param;
            paramValue = ""sv;
        }

        if (toRemove.find(paramName) == toRemove.end()) {
            // Include parameter
            params.emplace_back(paramName, paramValue);
        }

        startPos = ampPos + 1;
    }

    // Sort by query parameter name
    std::sort(params.begin(), params.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string result = std::move(basePath);
    if (!params.empty()) {
        bool firstParam = true;
        result.push_back('?');
        for (const auto& param : params) {
            if (!firstParam) {
                result += '&';
            } else {
                firstParam = false;
            }

            result.append(param.first);
            if (!param.second.empty()) {
                result.push_back('=');
                result.append(param.second);
            }
        }
    }

    return result;
}

std::string_view GetQueryFragmentOfPath(std::string_view fullPath) {
    size_t queryFragmentStart = fullPath.size();
    if (auto queryStart = fullPath.find('?'); queryStart != std::string_view::npos) {
        queryFragmentStart = std::min(queryFragmentStart, queryStart);
    }
    if (auto fragmentStart = fullPath.find('#'); fragmentStart != std::string_view::npos) {
        queryFragmentStart = std::min(queryFragmentStart, fragmentStart);
    }
    return fullPath.substr(queryFragmentStart);
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
    u.queryFragment = GetQueryFragmentOfPath(u.path);

    return u;
}

URL CanonicalizeURL(const URL& url) {
    URL canonical{};
    std::string canonicalFull;
    canonicalFull.reserve(url.url.size());

    // Lowercase scheme and host
    std::string scheme = url.scheme;
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return std::tolower(c); });
    canonical.scheme = scheme;

    std::string host = url.host;
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });
    canonical.host = host;

    canonicalFull += scheme + "://" + host;

    // Add non-default port
    if (!url.port.empty() && !((scheme == "http" && url.port == "80") || (scheme == "https" && url.port == "443"))) {
        canonicalFull += ":" + url.port;
        canonical.port = url.port;
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

    cleanPath = ResolvePath(cleanPath);  // Resolve directory . and ..
    cleanPath = CleanQueryParameters(cleanPath, DiscardURLQueryParameters);

    canonicalFull += cleanPath;
    canonical.path = cleanPath;
    canonical.queryFragment = GetQueryFragmentOfPath(cleanPath);

    canonical.url = std::move(canonicalFull);
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
