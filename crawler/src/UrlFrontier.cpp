#include "UrlFrontier.h"

#include "http/URL.h"

#include <cassert>

namespace mithril {

namespace {
constexpr size_t MaxUrlLength = 2048;
constexpr size_t MinUrlLength = 10;

bool HasInvalidChars(std::string_view str) {
    return std::any_of(str.begin(), str.end(), [](unsigned char c) { return c <= 0x20 || c > 0x7E; });
}

bool IsValidUrl(std::string_view url) {
    return url.length() >= MinUrlLength && url.length() <= MaxUrlLength && !HasInvalidChars(url);
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
        const auto& canonicalHost = it->second.canonicalHost;
        auto* robots = robotRulesCache_.GetOrFetch(canonicalHost);
        if (robots == nullptr) {
            // Still fetching...
            ++it;
            continue;
        }

        auto& urls = it->second.urls;
        bool pushed = false;
        for (auto& url : urls) {
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

    // Add to URLs seen before we go any farther
    seen_.Put(u);

    auto parsed = http::ParseURL(u);
    if (!parsed) {
        return false;
    }

    auto canonicalHost = http::CanonicalizeHost(*parsed);
    auto* robots = robotRulesCache_.GetOrFetch(canonicalHost);
    if (robots == nullptr) {
        // Fetch is in progress
        auto it = urlsWaitingForRobots_.find(canonicalHost.url);
        if (it == urlsWaitingForRobots_.end()) {
            auto insertResult =
                urlsWaitingForRobots_.insert({canonicalHost.url, WaitingURLs{.canonicalHost = canonicalHost}});
            assert(insertResult.second);
            it = insertResult.first;
            robotsCv_.notify_one();
        }

        it->second.urls.push_back(std::move(*parsed));
        return true;
    }

    if (!robots->Allowed(parsed->path)) {
        return false;
    }

    urls_.push(std::move(u));
    return true;
}

}  // namespace mithril
