#include "UrlFrontier.h"

#include <optional>

namespace mithril {

UrlFrontier::UrlFrontier() {}

bool UrlFrontier::Empty() const {
    std::unique_lock lock(mu_);
    return urls_.empty();
}

std::optional<std::string> UrlFrontier::GetURL() {
    std::unique_lock lock(mu_);

    if (urls_.empty()) {
        return std::nullopt;
    }

    auto u = std::move(urls_.front());
    urls_.pop();
    return u;
}

std::vector<std::string> UrlFrontier::GetURLs(size_t max) {
    if (max == 0) {
        return {};
    }

    std::unique_lock lock(mu_);
    cv_.wait(lock, [this]() { return !urls_.empty(); });

    std::vector<std::string> res;
    res.reserve(std::min(max, urls_.size()));
    while (!urls_.empty() && max != 0) {
        res.push_back(std::move(urls_.front()));
        urls_.pop();
        --max;
    }
    return res;
}

void UrlFrontier::PutURL(std::string u) {
    std::unique_lock lock(mu_);
    if (seen_.contains(u)) {
        return;
    }
    seen_.insert(u);
    urls_.push(std::move(u));
    cv_.notify_one();
}

}  // namespace mithril
