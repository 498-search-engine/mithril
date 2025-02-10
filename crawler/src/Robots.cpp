#include "Robots.h"

#include "Clock.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"
#include "http/URL.h"

#include <cstddef>
#include <iostream>
#include <string_view>

namespace mithril {

using namespace std::string_view_literals;

constexpr size_t MaxRobotsTxtSize = 500L * 1024L;  // 500 KB
constexpr auto MaxInFlightRobotsTxtRequests = 100;
constexpr int MaxRobotsTxtRedirects = 5;

namespace {

struct RobotLine {
    std::string_view directive;
    std::string_view value;
};

constexpr bool InsensitiveCharEquals(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

constexpr bool InsensitiveStrEquals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), InsensitiveCharEquals);
}

template<typename F>
void ConsumeUntil(std::string_view line, size_t& i, F f) {
    while (i < line.size() && !f(line[i])) {
        ++i;
    }
}

void ConsumeUntil(std::string_view line, size_t& i, char c) {
    while (i < line.size() && line[i] != c) {
        ++i;
    }
}

void ConsumeWhitespace(std::string_view line, size_t& i) {
    while (i < line.size() && std::isspace(line[i])) {
        ++i;
    }
}

void SortByLength(std::vector<std::string>& v) {
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
}

std::optional<RobotLine> ParseRobotLine(std::string_view line) {
    if (line.empty()) {
        return std::nullopt;
    }

    // Consume whitespace at start of line
    size_t i = 0;
    ConsumeWhitespace(line, i);
    if (i == line.size() || line[i] == '#') {
        // Empty line or comment
        return std::nullopt;
    }

    // Consume until ending of directive (whitespace or ':')
    size_t directiveStart = i;
    ConsumeUntil(line, i, [](char c) { return std::isspace(c) || c == ':'; });
    auto directive = line.substr(directiveStart, i - directiveStart);

    // Consume whitespace until ':', then whitespace after ':'
    ConsumeUntil(line, i, ':');
    ++i;  // consume :
    ConsumeWhitespace(line, i);

    if (i >= line.size()) {
        return std::nullopt;
    }

    // Consume rest of line until comment start or newline
    size_t valueStart = i;
    ConsumeUntil(line, i, [](char c) { return c == '#' || c == '\n' || c == '\r'; });

    // Backtrack to consume trailing whitespace, finding end of value
    size_t valueEnd = i;
    while (valueEnd > valueStart && std::isspace(line[valueEnd - 1])) {
        --valueEnd;
    }

    auto value = line.substr(valueStart, valueEnd - valueStart);

    return RobotLine{
        .directive = directive,
        .value = value,
    };
}

std::string_view FixWildcardPath(std::string_view p) {
    // Remove trailing '*' from "/something/*", we don't support wildcards
    if (p.ends_with("/*")) {
        p.remove_suffix(1);
    }
    return p;
}

}  // namespace


RobotRules::RobotRules() : RobotRules(true) {}

RobotRules::RobotRules(bool disallowAll) : disallowAll_(disallowAll) {}

RobotRules::RobotRules(std::vector<std::string> disallowPrefixes, std::vector<std::string> allowPrefixes)
    : disallowPrefixes_(std::move(disallowPrefixes)), allowPrefixes_(std::move(allowPrefixes)), disallowAll_(false) {
    SortByLength(disallowPrefixes_);
    SortByLength(allowPrefixes_);
}

RobotRules RobotRules::FromRobotsTxt(std::string_view file, std::string_view userAgent) {
    using namespace std::string_view_literals;

    std::vector<std::string> disallowPrefixes;
    std::vector<std::string> allowPrefixes;
    bool matchesUserAgent = false;
    bool inUserAgentDefns = false;

    if (file.size() > MaxRobotsTxtSize) {
        // Only parse MaxRobotsTxtSize amount of bytes
        file.remove_suffix(file.size() - MaxRobotsTxtSize);
    }

    size_t pos = 0;
    while (pos < file.size()) {
        auto lineEnd = file.find_first_of("\r\n", pos);
        if (lineEnd == std::string_view::npos) {
            lineEnd = file.size();
        }

        auto fileLine = file.substr(pos, lineEnd - pos);
        pos = lineEnd + 1;

        auto line = ParseRobotLine(fileLine);
        if (!line) {
            continue;
        }

        if (InsensitiveStrEquals(line->directive, "user-agent"sv)) {
            if (!inUserAgentDefns) {
                inUserAgentDefns = true;
                matchesUserAgent = false;
            }
            matchesUserAgent |= (line->value == "*" || InsensitiveStrEquals(line->value, userAgent));
            continue;
        } else {
            // Got a directive other than "User-agent", any subsequent
            // "User-agent" directives will begin a new group
            inUserAgentDefns = false;
        }

        if (!matchesUserAgent) {
            // Don't care about this rule
            continue;
        }

        if (InsensitiveStrEquals(line->directive, "disallow"sv)) {
            auto path = FixWildcardPath(line->value);
            disallowPrefixes.emplace_back(path);
        } else if (InsensitiveStrEquals(line->directive, "allow"sv)) {
            auto path = FixWildcardPath(line->value);
            allowPrefixes.emplace_back(path);
        }
    }

    return RobotRules{std::move(disallowPrefixes), std::move(allowPrefixes)};
}

bool RobotRules::Allowed(std::string_view path) const {
    if (disallowAll_) {
        return false;
    }

    if (disallowPrefixes_.empty() && allowPrefixes_.empty()) {
        return true;
    }

    bool allow = true;
    size_t longest = 0;

    // Check if any `Disallow` directive forbids this path
    for (const auto& disallow : disallowPrefixes_) {
        if (path.starts_with(disallow)) {
            longest = disallow.size();
            allow = false;
            break;
        }
    }

    // No `Disallow` directive for this URL, must be allowed
    if (allow) {
        return true;
    }

    // This url is disallowed. See if there is an `Allow` directive with longer
    // length than the `Disallow` directive to overrule it.
    for (const auto& allow : allowPrefixes_) {
        if (path.starts_with(allow) && allow.size() > longest) {
            return true;
        }
    }

    return false;
}

namespace {

std::string CanonicalizeHost(const std::string& scheme, const std::string& host, const std::string& port) {
    std::string h;
    h.append(scheme);
    h.append("://");
    h.append(host);
    if (port.empty()) {
        h.append(InsensitiveStrEquals(scheme, "https"sv) ? ":443" : ":80");
    } else {
        h.push_back(':');
        h.append(port);
    }

    return h;
}

std::string CanonicalizeHost(const http::URL& url) {
    std::string h;
    h.append(url.scheme);
    h.append("://");
    h.append(url.host);
    if (url.port.empty()) {
        h.append(InsensitiveStrEquals(url.scheme, "https"sv) ? ":443" : ":80");
    } else {
        h.push_back(':');
        h.append(url.port);
    }

    return h;
}

}  // namespace

RobotRules* RobotRulesCache::GetOrFetch(const std::string& scheme, const std::string& host, const std::string& port) {
    std::string canonicalHost = CanonicalizeHost(scheme, host, port);

    auto it = cache_.find(canonicalHost);
    if (it == cache_.end()) {
        if (executor_.InFlightRequests() < MaxInFlightRobotsTxtRequests) {
            cache_.insert({canonicalHost, {}});
            Fetch(scheme, host, port, canonicalHost);
        }
        return nullptr;
    }

    if (it->second.expiresAt == 0) {
        // Already fetching
        return nullptr;
    }

    auto now = MonotonicTime();
    if (now >= it->second.expiresAt) {
        it->second.expiresAt = 0;  // Mark as already fetching
        if (executor_.InFlightRequests() < MaxInFlightRobotsTxtRequests) {
            Fetch(scheme, host, port, canonicalHost);
        }
        return nullptr;
    }

    if (it->second.valid) {
        return &it->second.rules;
    }

    return nullptr;
}

void RobotRulesCache::Fetch(const std::string& scheme,
                            const std::string& host,
                            const std::string& port,
                            const std::string& canonicalHost) {
    executor_.Add(http::Request::GET(
        http::URL{
            .url = canonicalHost + "/robots.txt",
            .scheme = scheme,
            .host = host,
            .port = port,
            .path = "/robots.txt",
        },
        http::RequestOptions{
            .followRedirects = MaxRobotsTxtRedirects,
        }));
}

bool RobotRulesCache::HasPendingRequests() const {
    return executor_.InFlightRequests() > 0;
}

void RobotRulesCache::ProcessPendingRequests() {
    if (executor_.InFlightRequests() == 0) {
        return;
    }

    executor_.ProcessConnections();

    // Process connections with ready responses
    auto& ready = executor_.ReadyResponses();
    if (!ready.empty()) {
        for (const auto& r : ready) {
            HandleRobotsResponse(r);
        }
        ready.clear();
    }

    // Process requests that failed
    auto& failed = executor_.FailedRequests();
    if (!failed.empty()) {
        for (const auto& f : failed) {
            HandleRobotsResponseFailed(f);
        }
        failed.clear();
    }
}

void RobotRulesCache::HandleRobotsResponse(const http::CompleteResponse& r) {
    auto canonicalHost = CanonicalizeHost(r.req.Url());

    // TODO: when this is an LRU cache, could it be possible we don't find it?
    // Would it matter?
    auto it = cache_.find(canonicalHost);
    if (it == cache_.end()) {
        return;
    }

    auto header = http::ParseResponseHeader(r.res);
    if (!header) {
        std::cerr << "failed to parse robots.txt response for " << r.req.Url().url << std::endl;
        it->second.rules = RobotRules{true};
        it->second.valid = false;
        it->second.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
        return;
    }

    switch (header->status) {
    case http::StatusCode::OK:
        HandleRobotsOK(*header, r.res, it->second);
        break;

    case http::StatusCode::NotFound:
        HandleRobotsNotFound(it->second);
        break;

    case http::StatusCode::Unauthorized:
    case http::StatusCode::Forbidden:
    default:
        std::cerr << "got robots.txt status " << header->status << " for " << canonicalHost << std::endl;
        it->second.rules = RobotRules{true};
        it->second.valid = true;
        it->second.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
        break;

        // TODO: cut-outs for 429, 5xx, etc.
    }
}

void RobotRulesCache::HandleRobotsResponseFailed(const http::FailedRequest& failed) {
    auto canonicalHost = CanonicalizeHost(failed.req.Url());
    auto it = cache_.find(canonicalHost);
    if (it == cache_.end()) {
        return;
    }

    std::cerr << "request for robots.txt at " << canonicalHost << " failed" << std::endl;

    it->second.rules = RobotRules{};
    it->second.valid = false;
    it->second.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
}

void RobotRulesCache::HandleRobotsOK(const http::ResponseHeader& header,
                                     const http::Response& res,
                                     RobotCacheEntry& entry) {
    bool isTextPlain = header.ContentType != nullptr && header.ContentType->value.starts_with("text/plain");
    if (isTextPlain) {
        // Parse the response body
        entry.rules = RobotRules::FromRobotsTxt(std::string_view{res.body.data(), res.body.size()},
                                                "mithril-crawler"sv);  // TODO: move UA string somewhere else
        entry.valid = true;
    } else {
        entry.rules = RobotRules{false};
        entry.valid = true;
    }

    entry.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
}

void RobotRulesCache::HandleRobotsNotFound(RobotCacheEntry& entry) {
    // 404 Not Found = go for it!
    entry.rules = RobotRules{false};
    entry.valid = true;
    entry.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
}

}  // namespace mithril
