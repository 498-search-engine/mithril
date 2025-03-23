#include "UrlFrontier.h"

#include "CrawlerMetrics.h"
#include "Robots.h"
#include "ThreadSync.h"
#include "core/locks.h"
#include "core/optional.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

namespace {

bool HasInvalidChars(std::string_view str) {
    return std::any_of(str.begin(), str.end(), [](unsigned char c) { return c <= 0x20 || c > 0x7E; });
}

bool IsValidUrl(std::string_view url) {
    return url.length() >= http::MinUrlLength && url.length() <= http::MaxUrlLength && !HasInvalidChars(url);
}

constexpr unsigned int URLHighScoreCutoff = 80;
constexpr unsigned int URLHighScoreQueuePercent = 75;

}  //  namespace

UrlFrontier::UrlFrontier(const std::string& frontierDirectory, size_t concurrentRobotsRequests)
    : urlQueue_(frontierDirectory, URLHighScoreCutoff, URLHighScoreQueuePercent),
      robotRulesCache_(concurrentRobotsRequests) {}

void UrlFrontier::InitSync(ThreadSync& sync) {
    sync.RegisterCV(&robotsCv_);
    sync.RegisterCV(&freshURLsCv_);
    sync.RegisterCV(&urlQueueCv_);
}

size_t UrlFrontier::TotalSize() const {
    core::LockGuard queueLock(urlQueueMu_);
    return urlQueue_.TotalSize();
}

bool UrlFrontier::Empty() const {
    {
        core::LockGuard queueLock(urlQueueMu_);
        if (!urlQueue_.Empty()) {
            return false;
        }
    }
    {
        core::LockGuard lock(freshURLsMu_);
        if (!freshURLs_.empty()) {
            return false;
        }
    }
    return true;
}

bool UrlFrontier::CopyStateToDirectory(const std::string& directory) const {
    core::LockGuard lock(urlQueueMu_);
    return urlQueue_.CopyStateToDirectory(directory);
}

void UrlFrontier::RobotsRequestsThread(ThreadSync& sync) {
    while (true) {
        ProcessRobotsRequests(sync);
        if (sync.ShouldShutdown()) {
            break;
        }
        sync.MaybePause();
    }
    spdlog::info("frontier robots thread terminating");
}

void UrlFrontier::FreshURLsThread(ThreadSync& sync) {
    {
        core::LockGuard lock(urlQueueMu_);
        FrontierSize.Set(urlQueue_.TotalSize());
        FrontierQueueSize.Set(urlQueue_.Size());
    }
    {
        core::LockGuard lock(freshURLsMu_);
        FrontierFreshURLs.Set(freshURLs_.size());
    }

    while (true) {
        ProcessFreshURLs(sync);
        if (sync.ShouldShutdown()) {
            break;
        }
        sync.MaybePause();
    }
    spdlog::info("frontier fresh urls thread terminating");
}

void UrlFrontier::ProcessRobotsRequests(ThreadSync& sync) {
    {
        core::LockGuard lock(robotsCacheMu_);
        // Wait until we have requests to execute. We only ever get new requests
        // to process when ProcessRobotsRequests processes fresh URLs.
        robotsCv_.Wait(lock, [&]() { return robotRulesCache_.PendingRequests() > 0 || sync.ShouldSynchronize(); });
        if (sync.ShouldSynchronize()) {
            return;
        }

        size_t before = robotRulesCache_.PendingRequests();
        robotRulesCache_.ProcessPendingRequests();
        if (robotRulesCache_.PendingRequests() >= before) {
            // No requests finished on call to ProcessPendingRequests
            return;
        }
    }

    usleep(1);

    // Process completed robots.txt fetches
    std::set<std::string> allowedURLs;
    {
        core::LockGuard waitingLock(waitingUrlsMu_);
        core::LockGuard robotsLock(robotsCacheMu_);

        auto& completed = robotRulesCache_.CompletedFetchs();
        while (!completed.empty()) {
            auto it = urlsWaitingForRobots_.find(completed.back());
            completed.pop_back();
            if (it == urlsWaitingForRobots_.end()) {
                continue;
            }

            auto host = it->first;
            auto urls = std::move(it->second);
            urlsWaitingForRobotsCount_ -= urls.size();
            urlsWaitingForRobots_.erase(it);

            const auto* robots = robotRulesCache_.GetOrFetch(host);
            if (robots == nullptr) {
                // robots.txt page was invalid in some way, don't fetch any
                // results.
                continue;
            }

            // The cache has fetched and resolved the robots.txt page for this
            // canonical host. Process all queued URLs.
            for (auto& url : urls) {
                if (robots->Allowed(url.path)) {
                    allowedURLs.insert(std::move(url.url));
                }
            }
        }

        WaitingRobotsHosts.Set(urlsWaitingForRobots_.size());
        WaitingRobotsURLs.Set(urlsWaitingForRobotsCount_);
    }

    if (!allowedURLs.empty()) {
        core::LockGuard queueLock(urlQueueMu_);
        for (const auto& url : allowedURLs) {
            urlQueue_.PushURL(url);
        }
        urlQueueCv_.Broadcast();
        FrontierSize.Set(urlQueue_.TotalSize());
        FrontierQueueSize.Set(urlQueue_.Size());
    }
}

void UrlFrontier::GetURLs(ThreadSync& sync, size_t max, std::vector<std::string>& out, bool atLeastOne) {
    GetURLsFiltered(sync, max, out, [](std::string_view) { return true; });
}

void UrlFrontier::PushURL(std::string u) {
    core::LockGuard lock(freshURLsMu_);
    freshURLs_.push_back(std::move(u));
    FrontierFreshURLs.Set(freshURLs_.size());
    freshURLsCv_.Signal();
}

void UrlFrontier::PushURLs(std::vector<std::string>& urls) {
    core::LockGuard lock(freshURLsMu_);
    for (auto& url : urls) {
        freshURLs_.push_back(std::move(url));
    }
    FrontierFreshURLs.Set(freshURLs_.size());
    freshURLsCv_.Broadcast();
}

void UrlFrontier::ProcessFreshURLs(ThreadSync& sync) {
    // 0. Wait for fresh URLs
    std::vector<std::string> urls;
    {
        core::LockGuard freshLock(freshURLsMu_);
        freshURLsCv_.Wait(freshLock, [&]() { return !freshURLs_.empty() || sync.ShouldSynchronize(); });
        if (sync.ShouldSynchronize()) {
            return;
        }

        urls = std::move(freshURLs_);
        freshURLs_.clear();
        FrontierFreshURLs.Zero();
    }

    SPDLOG_TRACE("starting processing of {} fresh urls", urls.size());

    std::vector<http::URL> validURLs;
    validURLs.reserve(urls.size());

    // 1. Validate and parse URLs
    for (const auto& url : urls) {
        if (!IsValidUrl(url)) {
            continue;
        }
        auto parsed = http::ParseURL(url);
        if (!parsed) {
            continue;
        }
        validURLs.push_back(std::move(*parsed));
    }

    if (validURLs.empty()) {
        SPDLOG_TRACE("finished processing of fresh urls: no valid urls");
        return;
    }

    // 2. Discard already seen URLs
    std::vector<http::URL*> newURLs;
    newURLs.reserve(validURLs.size());
    {
        auto seen = std::set<std::string_view>{};
        core::LockGuard queueLock(urlQueueMu_);
        for (auto& url : validURLs) {
            if (seen.contains(url.url) || urlQueue_.Seen(url.url)) {
                continue;
            }
            seen.insert(url.url);
            newURLs.push_back(&url);
        }
    }

    if (newURLs.empty()) {
        SPDLOG_TRACE("finished processing of fresh urls: no new urls");
        return;
    }

    // 3. Compute canonical host names for robots.txt lookup
    std::vector<http::CanonicalHost> canonicalHosts;
    canonicalHosts.reserve(newURLs.size());
    std::transform(newURLs.begin(),
                   newURLs.end(),
                   std::back_inserter(canonicalHosts),
                   [](const http::URL* url) -> http::CanonicalHost { return http::CanonicalizeHost(*url); });

    enum class RobotsLookupResult : uint8_t { NotCached, Allowed, Disallowed };

    // 4. Look up robots.txt ruleset (if in memory)
    assert(newURLs.size() == canonicalHosts.size());
    std::vector<RobotsLookupResult> robotResults;
    robotResults.reserve(canonicalHosts.size());
    {
        core::LockGuard robotsLock(robotsCacheMu_);
        for (size_t i = 0; i < canonicalHosts.size(); ++i) {
            const auto* robots = robotRulesCache_.GetOrFetch(canonicalHosts[i]);
            if (robots == nullptr) {
                robotResults.push_back(RobotsLookupResult::NotCached);
            } else if (robots->Allowed(newURLs[i]->path)) {
                robotResults.push_back(RobotsLookupResult::Allowed);
            } else {
                robotResults.push_back(RobotsLookupResult::Disallowed);
            }
        }
    }

    // 5. Enqueue URLs that aren't ready to waiting list and discard disallowed URLs
    assert(newURLs.size() == robotResults.size());
    std::vector<http::URL*> pushURLs;
    pushURLs.reserve(newURLs.size());
    {
        core::LockGuard waitingLock(waitingUrlsMu_);
        for (size_t i = 0; i < newURLs.size(); ++i) {
            switch (robotResults[i]) {
            case RobotsLookupResult::NotCached:
                {
                    // robots.txt rules are not cached in memory. Push onto waiting
                    // queue.
                    size_t sizeBefore = urlsWaitingForRobots_.size();
                    urlsWaitingForRobots_[canonicalHosts[i]].push_back(std::move(*newURLs[i]));
                    if (sizeBefore != urlsWaitingForRobots_.size()) {
                        // Notify that a new request for a robots.txt page is available
                        robotsCv_.Signal();
                    }
                    ++urlsWaitingForRobotsCount_;
                    break;
                }
            case RobotsLookupResult::Allowed:
                pushURLs.push_back(newURLs[i]);
                break;
            case RobotsLookupResult::Disallowed:
                // Do nothing
                break;
            }
        }
        WaitingRobotsHosts.Set(urlsWaitingForRobots_.size());
        WaitingRobotsURLs.Set(urlsWaitingForRobotsCount_);
    }

    if (pushURLs.empty()) {
        SPDLOG_TRACE("finished processing of fresh urls: no ready urls, {} awaiting robots.txt", newURLs.size());
        return;
    }

    // 6. Push all allowed, ready to fetch URLs onto frontier
    {
        core::LockGuard queueLock(urlQueueMu_);
        for (auto* url : pushURLs) {
            urlQueue_.PushURL(url->url);
        }
        FrontierSize.Set(urlQueue_.TotalSize());
        FrontierQueueSize.Set(urlQueue_.Size());
    }
    urlQueueCv_.Broadcast();

    SPDLOG_TRACE("finished processing of fresh urls: {} urls pushed, {} awaiting robots.txt",
                 pushURLs.size(),
                 newURLs.size() - pushURLs.size());
}

void UrlFrontier::DumpPendingURLs(std::vector<std::string>& urls) {
    {
        core::LockGuard freshLock(freshURLsMu_);
        for (const auto& url : freshURLs_) {
            urls.push_back(url);
        }
    }
    {
        core::LockGuard waitingLock(waitingUrlsMu_);
        for (const auto& entry : urlsWaitingForRobots_) {
            for (const auto& url : entry.second) {
                urls.push_back(url.url);
            }
        }
    }
}

void UrlFrontier::TouchRobotRequestTimeouts() {
    core::LockGuard robotsLock(robotsCacheMu_);
    robotRulesCache_.TouchRobotRequestTimeouts();
}

core::Optional<unsigned long> UrlFrontier::LookUpCrawlDelayNonblocking(const http::CanonicalHost& host,
                                                                       unsigned long defaultDelay) {
    core::LockGuard lock(robotsCacheMu_, core::DeferLock);
    if (!lock.TryLock()) {
        return core::nullopt;
    }

    const auto* res = robotRulesCache_.GetOrFetch(host);
    if (res == nullptr) {
        return core::nullopt;
    }
    return res->CrawlDelay().ValueOr(defaultDelay);
}

}  // namespace mithril
