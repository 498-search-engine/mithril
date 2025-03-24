#include "Robots.h"

#include "Clock.h"
#include "CrawlerMetrics.h"
#include "Util.h"
#include "core/optional.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

using namespace std::string_view_literals;

constexpr size_t MaxRobotsTxtSize = 500L * 1024L;  // 500 KB
constexpr int MaxRobotsTxtRedirects = 5;
constexpr long RobotsTxtRequestTimeoutSeconds = 10;

namespace {

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

}  // namespace

namespace internal {

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

RobotDirectives ParseRobotsTxt(std::string_view file, std::string_view userAgent) {
    RobotDirectives d;
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
            d.disallows.emplace_back(line->value);
        } else if (InsensitiveStrEquals(line->directive, "allow"sv)) {
            d.allows.emplace_back(line->value);
        } else if (InsensitiveStrEquals(line->directive, "crawl-delay"sv)) {
            try {
                d.crawlDelay = std::stoul(std::string{line->value});
            } catch (const std::invalid_argument&) {  // NOLINT(bugprone-empty-catch)
            } catch (const std::out_of_range&) {      // NOLINT(bugprone-empty-catch)
            }
        }
    }

    return d;
}


bool RobotsTrie::MatchResult::operator>(const MatchResult& other) const {
    if (length != other.length) {
        return length > other.length;
    }
    // Allow (2) > Disallow (1)
    return type > other.type;
}

RobotsTrie::RobotsTrie(const std::vector<std::string>& disallows, const std::vector<std::string>& allows) {
    // Insert all patterns, disallows first (allows take precedence for same length)
    for (const auto& pattern : disallows) {
        Insert(pattern, NodeType::Disallow);
    }
    for (const auto& pattern : allows) {
        Insert(pattern, NodeType::Allow);
    }
}

void RobotsTrie::Insert(const std::string& pattern, NodeType type) {
    if (pattern.empty() || pattern.length() > http::MaxUrlLength) {
        return;
    }

    // Split path into segments on / delimiter
    auto segments = SplitPath(pattern);
    if (segments.size() > 50) {
        // Too long, don't bother
        return;
    }

    for (size_t i = 0; i < segments.size(); ++i) {
        auto segment = segments[i];
        if (segment.size() > 1) {
            auto starPos = segment.find('*');
            if (starPos != std::string_view::npos && starPos != segment.size() - 1 && i != segment.size() - 1) {
                // Segment contains a '*' but not just the star, or at the end -- we
                // can't handle that with our trie implementation. Discard the rule.
                return;
            }
        }
    }

    Node* current = &root_;

    for (size_t i = 0; i < segments.size(); ++i) {
        auto segment = segments[i];
        if (segment.empty()) {
            if (!current->emptyMatch) {
                current->emptyMatch = std::make_unique<Node>();
            }
            current = current->emptyMatch.get();
        } else if (segment == "*") {
            if (!current->wildcardMatch) {
                current->wildcardMatch = std::make_unique<Node>();
            }
            current = current->wildcardMatch.get();
        } else {
            bool isTrailingWildcard = false;
            if (i == segments.size() - 1 && segment.back() == '*') {
                // Trailing wildcard
                segment.remove_suffix(1);
                isTrailingWildcard = true;
            }

            // Find or insert segment in sorted vector
            auto it = std::lower_bound(current->fixedSegments.begin(),
                                       current->fixedSegments.end(),
                                       segment,
                                       [](const auto& pair, const std::string_view& val) { return pair.first < val; });

            if (it == current->fixedSegments.end() || it->first != segment) {
                it = current->fixedSegments.insert(it, {std::string{segment}, Node{}});
            }
            current = &it->second;

            if (isTrailingWildcard) {
                current->trailingWildcard = true;
            }
        }
    }

    current->type = type;
    current->patternLength = static_cast<uint16_t>(pattern.length());
}

bool RobotsTrie::IsAllowed(std::string_view path) const {
    auto segments = SplitPath(path);
    auto result = FindBestMatch(segments);
    // Allow if no `Disallow` rule matched
    return result.type != NodeType::Disallow;
}

void RobotsTrie::FindBestMatchRecursive(const std::vector<std::string_view>& segments,
                                        size_t index,
                                        const Node* node,
                                        MatchResult& best) const {
    assert(node != nullptr);

    // Check if current node is terminal and updates best if appropriate
    if (node->type != NodeType::NonTerminal) {
        MatchResult current{
            .type = node->type,
            .length = node->patternLength,
        };
        if (current > best) {
            best = current;
        }
    }

    // If we've processed all segments, stop recursion
    if (index >= segments.size()) {
        return;
    }

    constexpr auto Comparator = [](const auto& pair, const std::string_view& val) { return pair.first < val; };
    auto firstLetter = segments[index].substr(0, 1);  // segments[index] could be empty! substr will check.

    // Get range of fixed segments based on the first letter.
    // There are two interesting cases:
    // 1. Exact match
    // 2. Prefix match
    // Prefix match is only possible if the node is terminal.
    auto begin = std::lower_bound(node->fixedSegments.begin(), node->fixedSegments.end(), firstLetter, Comparator);
    auto end = node->fixedSegments.end();
    if (!firstLetter.empty()) {
        char nextLetter = firstLetter[0] + 1;
        end = std::lower_bound(begin, node->fixedSegments.end(), std::string_view{&nextLetter, 1}, Comparator);
    }

    for (auto it = begin; it != end; ++it) {
        assert(!it->first.empty());
        if (it->first == segments[index]) {
            // Exact match
            FindBestMatchRecursive(segments, index + 1, &it->second, best);
        } else if (it->second.type != NodeType::NonTerminal) {
            // Non-terminal, check if segment is prefix
            if (segments[index].starts_with(it->first)) {
                FindBestMatchRecursive(segments, index + 1, &it->second, best);
            }
        }
    }

    // Segment wildcard
    if (node->wildcardMatch) {
        FindBestMatchRecursive(segments, index + 1, node->wildcardMatch.get(), best);
    }

    // Empty match, i.e. trailing /
    if (node->emptyMatch) {
        FindBestMatchRecursive(segments, index + 1, node->emptyMatch.get(), best);
    }

    // Trailing wildcard
    if (node->trailingWildcard) {
        MatchResult current{
            .type = node->type,
            .length = node->patternLength,
        };
        if (current > best) {
            best = current;
        }
    }
}

RobotsTrie::MatchResult RobotsTrie::FindBestMatch(const std::vector<std::string_view>& segments) const {
    MatchResult best;
    FindBestMatchRecursive(segments, 0, &root_, best);
    return best;
}

}  // namespace internal


RobotRules::RobotRules() : RobotRules(true) {}

RobotRules RobotRules::AllowAll() {
    return RobotRules{false};
}

RobotRules RobotRules::DisallowAll() {
    return RobotRules{true};
}

RobotRules::RobotRules(bool disallowAll) : trie_(nullptr), disallowAll_(disallowAll) {}

RobotRules::RobotRules(const std::vector<std::string>& disallowPrefixes,
                       const std::vector<std::string>& allowPrefixes,
                       core::Optional<unsigned long> crawlDelay)
    : trie_(nullptr), disallowAll_(false), crawlDelay_(crawlDelay) {
    if (allowPrefixes.size() == 0 && disallowPrefixes.size() == 1) {
        if (disallowPrefixes.front().empty() || disallowPrefixes.front() == "/"sv) {
            // Common "Disallow everything" case
            disallowAll_ = true;
            return;
        }
    }

    trie_ = std::make_unique<internal::RobotsTrie>(disallowPrefixes, allowPrefixes);
}

RobotRules RobotRules::FromRobotsTxt(std::string_view file, std::string_view userAgent) {
    auto directives = internal::ParseRobotsTxt(file, userAgent);
    return RobotRules{directives.disallows, directives.allows, directives.crawlDelay};
}

bool RobotRules::Allowed(std::string_view path) const {
    if (disallowAll_) {
        return false;
    }

    if (trie_ == nullptr) {
        return true;
    }

    return trie_->IsAllowed(path);
}

const core::Optional<unsigned long>& RobotRules::CrawlDelay() const {
    return crawlDelay_;
}

RobotRulesCache::RobotRulesCache(size_t maxInFlightRequests, size_t cacheSize)
    : maxInFlightRequests_(maxInFlightRequests), cache_(cacheSize) {}

const RobotRules* RobotRulesCache::GetOrFetch(const http::CanonicalHost& canonicalHost) {
    auto* entry = cache_.Find(canonicalHost.url);
    if (entry == nullptr) {
        cache_.Insert({canonicalHost.url, RobotCacheEntry{}});
        QueueFetch(canonicalHost);
        return nullptr;
    }

    if (entry->second.expiresAt == 0) {
        // Already fetching
        return nullptr;
    }

    auto now = MonotonicTime();
    if (now >= entry->second.expiresAt) {
        entry->second.expiresAt = 0;  // Mark as already fetching
        QueueFetch(canonicalHost);
        return nullptr;
    }

    return &entry->second.rules;
}

void RobotRulesCache::QueueFetch(const http::CanonicalHost& canonicalHost) {
    queuedFetches_.push(canonicalHost);
}

void RobotRulesCache::Fetch(const http::CanonicalHost& canonicalHost) {
    SPDLOG_TRACE("starting robots.txt request: {}", canonicalHost.host);
    executor_.Add(http::Request::GET(
        http::URL{
            .url = canonicalHost.url + "/robots.txt",
            .scheme = canonicalHost.scheme,
            .host = canonicalHost.host,
            .port = canonicalHost.port,
            .path = "/robots.txt",
        },
        http::RequestOptions{
            .followRedirects = MaxRobotsTxtRedirects,
            .timeout = RobotsTxtRequestTimeoutSeconds,
            .maxResponseSize = MaxRobotsTxtSize,
            .enableCompression = true,
        }));
}

size_t RobotRulesCache::PendingRequests() const {
    return executor_.InFlightRequests() + queuedFetches_.size();
}

void RobotRulesCache::FillFromQueue() {
    while (!queuedFetches_.empty() && executor_.InFlightRequests() < maxInFlightRequests_) {
        Fetch(queuedFetches_.front());
        queuedFetches_.pop();
    }
    InFlightRobotsRequestsMetric.Set(executor_.InFlightRequests());
}

void RobotRulesCache::ProcessPendingRequests() {
    FillFromQueue();

    if (executor_.InFlightRequests() == 0) {
        return;
    }

    executor_.ProcessConnections();

    // Process connections with ready responses
    auto& ready = executor_.ReadyResponses();
    if (!ready.empty()) {
        for (auto& r : ready) {
            HandleRobotsResponse(std::move(r));
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

void RobotRulesCache::HandleRobotsResponse(http::CompleteResponse r) {
    RobotsResponseCodesMetric
        .WithLabels({
            {"status", std::to_string(r.res.header.status)}
    })
        .Inc();

    auto canonicalHost = CanonicalizeHost(r.req.Url());
    SPDLOG_TRACE("successful robots.txt request: {}", canonicalHost.host);

    auto& entry = cache_[canonicalHost.url];
    try {
        // Decode the body if it is encoded.
        r.res.DecodeBody();
    } catch (const std::runtime_error& e) {
        // Something went wrong while decoding
        spdlog::warn("encountered error while decoding body for {}: {}", r.req.Url().url, e.what());
        entry.rules = RobotRules::DisallowAll();
        entry.expiresAt = MonotonicTime() + RobotsTxtCacheFailureDurationSeconds;
        completedFetches_.push_back(std::move(canonicalHost));
        return;
    }

    switch (r.res.header.status) {
    case http::StatusCode::OK:
        HandleRobotsOK(r.res.header, r.res, entry);
        break;

    case http::StatusCode::NotFound:
        HandleRobotsNotFound(entry);
        break;

    case http::StatusCode::BadRequest:
    case http::StatusCode::Unauthorized:
    case http::StatusCode::Forbidden:
    default:
        spdlog::info("got robots.txt status {} for {}", r.res.header.status, canonicalHost.url);
        entry.rules = RobotRules::DisallowAll();
        entry.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
        break;

        // TODO: cut-outs for 429, 5xx, etc.
    }

    completedFetches_.push_back(std::move(canonicalHost));
}

void RobotRulesCache::HandleRobotsResponseFailed(const http::FailedRequest& failed) {
    auto canonicalHost = CanonicalizeHost(failed.req.Url());
    SPDLOG_TRACE("failed robots.txt request: {} {}", canonicalHost.host, http::StringOfRequestError(failed.error));

    cache_[canonicalHost.url] = RobotCacheEntry{
        .rules = RobotRules::DisallowAll(),
        .expiresAt = MonotonicTime() + RobotsTxtCacheFailureDurationSeconds,
    };

    completedFetches_.push_back(std::move(canonicalHost));
}

void RobotRulesCache::HandleRobotsOK(const http::ResponseHeader& header,
                                     const http::Response& res,
                                     RobotCacheEntry& entry) {
    bool isTextPlain = header.ContentType != nullptr && header.ContentType->value.starts_with("text/plain");
    if (isTextPlain) {
        // Parse the response body
        entry.rules = RobotRules::FromRobotsTxt(std::string_view{res.body.data(), res.body.size()},
                                                "mithril-crawler"sv);  // TODO: move UA string somewhere else
    } else {
        entry.rules = RobotRules::AllowAll();
    }

    entry.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
}

void RobotRulesCache::HandleRobotsNotFound(RobotCacheEntry& entry) {
    // 404 Not Found = go for it!
    entry.rules = RobotRules::AllowAll();
    entry.expiresAt = MonotonicTime() + RobotsTxtCacheDurationSeconds;
}

void RobotRulesCache::TouchRobotRequestTimeouts() {
    executor_.TouchRequestTimeouts();
}

std::vector<http::CanonicalHost>& RobotRulesCache::CompletedFetchs() {
    return completedFetches_;
}

}  // namespace mithril
