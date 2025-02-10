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
    robotsCv_.wait(lock, [this]() { return !urlsWaitingForRobots_.empty() || robotRulesCache_.HasPendingRequests(); });

    if (robotRulesCache_.HasPendingRequests()) {
        robotRulesCache_.ProcessPendingRequests();
    }

    auto it = urlsWaitingForRobots_.begin();
    while (it != urlsWaitingForRobots_.end()) {
        auto* robots = robotRulesCache_.GetOrFetch(it->scheme, it->host, it->port);
        if (robots == nullptr) {
            // Still fetching...
            ++it;
            continue;
        }

        if (robots->Allowed(it->path)) {
            seen_.Put(it->url);
            urls_.push(std::move(it->url));
            cv_.notify_one();
        } else {
            // Not allowed
            std::cerr << "url " << it->url << " disallowed by robots.txt" << std::endl;
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

    auto* robots = robotRulesCache_.GetOrFetch(parsed->scheme, parsed->host, parsed->port);
    if (robots == nullptr) {
        // Fetch is in progress
        urlsWaitingForRobots_.push_back(std::move(*parsed));
        robotsCv_.notify_one();
        return true;
    }

    if (!robots->Allowed(parsed->path)) {
        std::cerr << "url " << u << " disallowed by robots.txt" << std::endl;
        return false;
    }

    seen_.Put(u);
    urls_.push(std::move(u));
    return true;
}

}  // namespace mithril
