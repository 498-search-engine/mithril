#include "UrlFrontier.h"

#include "http/URL.h"

#include <cassert>

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
    std::unique_lock lock(mu_);
    return urls_.empty();
}

void UrlFrontier::ProcessRobotsRequests() {
    std::unique_lock lock(mu_);

    // Wait until we have requests to execute. We only ever get new requests to
    // process when PutURLInternal is called.
    robotsCv_.wait(lock, [this]() { return robotRulesCache_.PendingRequests() > 0; });

    size_t before = robotRulesCache_.PendingRequests();
    robotRulesCache_.ProcessPendingRequests();
    if (robotRulesCache_.PendingRequests() >= before) {
        // No requests finished on call to ProcessPendingRequests
        return;
    }

    auto it = urlsWaitingForRobots_.begin();
    while (it != urlsWaitingForRobots_.end()) {
        auto* robots = robotRulesCache_.GetOrFetch(it->first);
        if (robots == nullptr) {
            // Still fetching...
            ++it;
            continue;
        }

        // The cache has fetched and resolved the robots.txt page for this
        // canonical host. Process all queued URLs.
        bool pushed = false;
        for (auto& url : it->second) {
            if (robots->Allowed(url.path)) {
                assert(seen_.Contains(url.url));
                urls_.push(std::move(url.url));
                pushed = true;
            }
        }

        if (pushed) {
            cv_.notify_all();
        }

        it = urlsWaitingForRobots_.erase(it);
    }
}

void UrlFrontier::GetURLs(size_t max, std::vector<std::string>& out, bool atLeastOne) {
    if (max == 0) {
        return;
    }

    std::unique_lock lock(mu_);
    if (atLeastOne) {
        cv_.wait(lock, [this]() { return !urls_.empty(); });
    } else if (urls_.empty()) {
        return;
    }

    out.reserve(std::min(max, urls_.size()));
    while (!urls_.empty() && max != 0) {
        out.push_back(std::move(urls_.front()));
        urls_.pop();
        --max;
    }
}

void UrlFrontier::PutURL(std::string u) {
    std::unique_lock lock(mu_);
    if (PutURLInternal(std::move(u))) {
        cv_.notify_one();
    }
}

void UrlFrontier::PutURLs(std::vector<std::string> urls) {
    std::unique_lock lock(mu_);

    int i = 0;
    for (auto& u : urls) {
        if (PutURLInternal(std::move(u))) {
            ++i;
        }
    }

    if (i > 0) {
        cv_.notify_all();
    }
}

bool UrlFrontier::PutURLInternal(std::string u) {
    if (!IsValidUrl(u) || seen_.Contains(u)) {
        return false;
    }

    auto parsed = http::ParseURL(u);
    if (!parsed) {
        return false;
    }

    // Add to URLs seen set before we go any further.
    seen_.Put(u);

    auto canonicalHost = http::CanonicalizeHost(*parsed);
    auto* robots = robotRulesCache_.GetOrFetch(canonicalHost);
    if (robots == nullptr) {
        // Fetch is in progress
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
        return true;
    }

    if (!robots->Allowed(parsed->path)) {
        return false;
    }

    urls_.push(std::move(u));
    return true;
}

}  // namespace mithril
