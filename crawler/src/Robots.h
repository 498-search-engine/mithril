#ifndef CRAWLER_ROBOTS_H
#define CRAWLER_ROBOTS_H

#include "http/RequestExecutor.h"
#include "http/Response.h"
#include "http/URL.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mithril {

constexpr long RobotsTxtCacheDurationSeconds = 4L * 60L * 60L;  // 4 hours

class RobotRulesCache;

class RobotRules {
public:
    RobotRules();
    RobotRules(bool disallowAll);
    RobotRules(std::vector<std::string> disallowPrefixes, std::vector<std::string> allowPrefixes);

    static RobotRules FromRobotsTxt(std::string_view file, std::string_view userAgent);

    bool Allowed(std::string_view path) const;

private:
    // TODO: would it be better to use a trie or some other data structure?
    std::vector<std::string> disallowPrefixes_;  // sorted by length descending
    std::vector<std::string> allowPrefixes_;     // sorted by length descending
    bool disallowAll_;
};


class RobotRulesCache {
public:
    RobotRulesCache() = default;

    RobotRules* GetOrFetch(const http::URL& canonicalHost);

    size_t PendingRequests() const;
    void ProcessPendingRequests();

private:
    struct RobotCacheEntry {
        RobotRules rules;
        long expiresAt{0L};
        bool valid{false};
    };

    void Fetch(const http::URL& canonicalHost);

    void HandleRobotsResponse(const http::CompleteResponse& r);
    void HandleRobotsResponseFailed(const http::FailedRequest& failed);

    static void HandleRobotsOK(const http::ResponseHeader& header, const http::Response& res, RobotCacheEntry& entry);
    static void HandleRobotsNotFound(RobotCacheEntry& entry);

    // TODO: Use an LRU cache
    std::unordered_map<std::string, RobotCacheEntry> cache_;
    http::RequestExecutor executor_;
};

}  // namespace mithril

#endif
