#include "UrlFrontier.h"

#include "Robots.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
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

}  //  namespace

UrlFrontier::UrlFrontier() {}

bool UrlFrontier::Empty() const {
    std::unique_lock lock(urlQueueMu_);
    return urls_.empty();
}

void UrlFrontier::ProcessRobotsRequests() {
    {
        std::unique_lock lock(robotsCacheMu_);
        // Wait until we have requests to execute. We only ever get new requests
        // to process when PutURLInternal is called.
        robotsCv_.wait(lock, [this]() { return robotRulesCache_.PendingRequests() > 0; });

        size_t before = robotRulesCache_.PendingRequests();
        robotRulesCache_.ProcessPendingRequests();
        if (robotRulesCache_.PendingRequests() >= before) {
            // No requests finished on call to ProcessPendingRequests
            return;
        }
    }

    // Process waiting URLs
    std::unique_lock waitingLock(waitingUrlsMu_);
    std::vector<std::string> allowedURLs;

    auto it = urlsWaitingForRobots_.begin();
    while (it != urlsWaitingForRobots_.end()) {
        {
            std::unique_lock robotsLock(robotsCacheMu_);
            const auto* robots = robotRulesCache_.GetOrFetch(it->first);
            if (robots == nullptr) {
                // Still fetching...
                ++it;
                continue;
            }

            // The cache has fetched and resolved the robots.txt page for this
            // canonical host. Process all queued URLs.
            for (auto& url : it->second) {
                if (robots->Allowed(url.path)) {
                    allowedURLs.push_back(url.url);
                }
            }

            if (!allowedURLs.empty()) {
                std::unique_lock queueLock(urlQueueMu_);
                for (auto& url : allowedURLs) {
                    urls_.push(std::move(url));
                }
                allowedURLs.clear();
                urlQueueCv_.notify_all();
            }
        }
        it = urlsWaitingForRobots_.erase(it);
    }
}

void UrlFrontier::GetURLs(size_t max, std::vector<std::string>& out, bool atLeastOne) {
    if (max == 0) {
        return;
    }

    std::unique_lock lock(urlQueueMu_, std::defer_lock);
    if (atLeastOne) {
        // The caller wants at least one URL, we need to wait until we have at
        // least one.
        lock.lock();
        urlQueueCv_.wait(lock, [this]() { return !urls_.empty(); });
    } else {
        // The caller doesn't care if we can't get a URL. If we can't grab the
        // lock right now, we won't wait around.
        if (!lock.try_lock()) {
            return;
        }
        if (urls_.empty()) {
            return;
        }
    }

    out.reserve(std::min(max, urls_.size()));
    while (!urls_.empty() && max != 0) {
        out.push_back(std::move(urls_.front()));
        urls_.pop();
        --max;
    }
}

void UrlFrontier::PutURL(std::string u) {
    std::unique_lock lock(freshURLsMu_);
    freshURLs_.push_back(std::move(u));
    freshURLsCv_.notify_one();
}

void UrlFrontier::PutURLs(std::vector<std::string>& urls) {
    std::unique_lock lock(freshURLsMu_);
    for (auto& url : urls) {
        freshURLs_.push_back(std::move(url));
    }
    freshURLsCv_.notify_all();
}

void UrlFrontier::ProcessFreshURLs() {
    // 0. Wait for fresh URLs
    std::vector<std::string> urls;
    {
        std::unique_lock freshLock(freshURLsMu_);
        freshURLsCv_.wait(freshLock, [this]() { return !freshURLs_.empty(); });
        urls = std::move(freshURLs_);
        freshURLs_.clear();
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
        std::unique_lock seenLock(seenMu_);
        for (auto& url : validURLs) {
            if (seen_.Contains(url.url)) {
                continue;
            }
            seen_.Put(url.url);
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
        std::unique_lock robotsLock(robotsCacheMu_);
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
        std::unique_lock waitingLock(waitingUrlsMu_);
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
                        robotsCv_.notify_one();
                    }
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
    }

    if (pushURLs.empty()) {
        SPDLOG_TRACE("finished processing of fresh urls: no ready urls, {} awaiting robots.txt", newURLs.size());
        return;
    }

    // 6. Push all allowed, ready to fetch URLs onto frontier
    {
        std::unique_lock queueLock(urlQueueMu_);
        for (auto* url : pushURLs) {
            urls_.push(std::move(url->url));
        }
    }
    urlQueueCv_.notify_all();

    SPDLOG_TRACE("finished processing of fresh urls: {} urls pushed, {} awaiting robots.txt",
                 pushURLs.size(),
                 newURLs.size() - pushURLs.size());
}

}  // namespace mithril
