#include "UrlFrontier.h"


namespace mithril {

UrlFrontier::UrlFrontier() {}

bool UrlFrontier::Empty() const {
    std::unique_lock lock(mu_);
    return urls_.empty();
}

void UrlFrontier::GetURLs(size_t max, std::vector<std::string>& out) {
    if (max == 0) {
        return;
    }

    std::unique_lock lock(mu_);
    cv_.wait(lock, [this]() { return !urls_.empty(); });

    out.reserve(std::min(max, urls_.size()));
    while (!urls_.empty() && max != 0) {
        out.push_back(std::move(urls_.front()));
        urls_.pop();
        --max;
    }
}

int UrlFrontier::PutURL(std::string u) {
    std::unique_lock lock(mu_);
    if (seen_.Contains(u)) {
        return 0;
    }
    seen_.Put(u);
    urls_.push(std::move(u));
    cv_.notify_one();
    return 1;
}

int UrlFrontier::PutURLs(std::vector<std::string> urls) {
    std::unique_lock lock(mu_);

    int i = 0;
    for (auto& u : urls) {
        if (seen_.Contains(u)) {
            continue;
        }
        seen_.Put(u);
        urls_.push(std::move(u));
        ++i;
    }

    cv_.notify_all();
    return i;
}

}  // namespace mithril
