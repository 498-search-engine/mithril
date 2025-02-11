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

class RobotRules {
public:
    /**
     * @brief Creates a RobotRules object that disallows all paths.
     */
    RobotRules();

    /**
     * @brief Creates a RobotRules object that either allows or disallows all
     * paths.
     *
     * @param disallowAll Whether to disallow all paths.
     */
    RobotRules(bool disallowAll);

    /**
     * @brief Creates a RobotRules object from a list of disallowed prefixes and
     * allowed prefixes.
     *
     * @param disallowPrefixes Disallowed prefixes to filter by.
     * @param allowPrefixes Allowed prefixes to filter by.
     */
    RobotRules(std::vector<std::string> disallowPrefixes, std::vector<std::string> allowPrefixes);

    /**
     * @brief Creates a RobotRules object from the contents of a robots.txt file.
     *
     * @param file robots.txt file contents
     * @param userAgent User-Agent of parsing entity
     * @return RobotRules Parsed ruleset
     */
    static RobotRules FromRobotsTxt(std::string_view file, std::string_view userAgent);

    /**
     * @brief Determines whether a path is allowed based on the ruleset. Matches
     * are made against Disallow and Allow directives by taking the longest
     * matching path rule.
     *
     * @param path Path to check against.
     */
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

    /**
     * @brief Gets the ruleset associated with the canonical host, or queues up
     * a request to fetch the robots.txt ruleset.
     *
     * @param canonicalHost Canonical host URL to get the ruleset for
     * @return RobotRules* The ruleset, or nullptr if not yet in the cache.
     */
    RobotRules* GetOrFetch(const http::CanonicalHost& canonicalHost);

    /**
     * @brief Returns the number of pending robots.txt requests.
     */
    size_t PendingRequests() const;

    /**
     * @brief Processes pending robots.txt requests.
     */
    void ProcessPendingRequests();

private:
    struct RobotCacheEntry {
        RobotRules rules;
        long expiresAt{0L};
        bool valid{false};
    };

    /**
     * @brief Triggers a fetch for the robots.txt page of a host.
     *
     * @param canonicalHost Canonical host to fetch for.
     */
    void Fetch(const http::CanonicalHost& canonicalHost);

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
