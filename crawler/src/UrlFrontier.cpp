#include "UrlFrontier.h"

#include "Robots.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
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
            auto* robots = robotRulesCache_.GetOrFetch(it->first);
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
    if (PutURLInternal(std::move(u))) {
        urlQueueCv_.notify_one();
    }
}

void UrlFrontier::PutURLs(std::vector<std::string> urls) {
    int i = 0;
    for (auto& u : urls) {
        if (PutURLInternal(std::move(u))) {
            ++i;
        }
    }

    if (i > 0) {
        urlQueueCv_.notify_all();
    }
}

bool UrlFrontier::PutURLInternal(std::string u) {
    if (!IsValidUrl(u)) {
        return false;
    }

    {
        std::unique_lock seenLock(seenMu_);
        if (seen_.Contains(u)) {
            return false;
        }
        seen_.Put(u);
    }

    auto parsed = http::ParseURL(u);
    if (!parsed) {
        return false;
    }

    // Add to URLs seen set before we go any further.
    auto canonicalHost = http::CanonicalizeHost(*parsed);

    RobotRules* robots = nullptr;
    {
        std::unique_lock robotsLock(robotsCacheMu_);
        robots = robotRulesCache_.GetOrFetch(canonicalHost);
    }

    if (robots == nullptr) {
        std::unique_lock waitingLock(waitingUrlsMu_);
        auto it = urlsWaitingForRobots_.find(canonicalHost);
        if (it == urlsWaitingForRobots_.end()) {
            // This is the first pending URL for this canonical host
            auto insertResult = urlsWaitingForRobots_.try_emplace(std::move(canonicalHost));
            assert(insertResult.second);
            it = insertResult.first;
            // Notify that a new request for a robots.txt page is available
            robotsCv_.notify_one();
        }
        it->second.push_back(std::move(*parsed));
        return false;
    } else if (!robots->Allowed(parsed->path)) {
        return false;
    }

    std::unique_lock queueLock(urlQueueMu_);
    urls_.push(std::move(u));
    return true;
}

}  // namespace mithril
