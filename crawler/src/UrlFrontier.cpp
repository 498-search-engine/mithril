#include "UrlFrontier.h"

namespace mithril {

UrlFrontier::UrlFrontier() {}

namespace {
    constexpr size_t MAX_URL_LENGTH = 2048;
    constexpr size_t MIN_URL_LENGTH = 10;
    
    bool HasInvalidChars(std::string_view str) {
        return std::any_of(str.begin(), str.end(), 
            [](unsigned char c) {
                return c <= 0x20 || c > 0x7E;
            });
    }
    
    bool IsValidUrl(std::string_view url) {
        return url.length() >= MIN_URL_LENGTH &&  url.length() <= MAX_URL_LENGTH &&!HasInvalidChars(url);
    }
}  //  namespace

bool UrlFrontier::Empty() const {
    std::unique_lock lock(mu_);
    return urls_.empty();
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

int UrlFrontier::PutURL(std::string u) {
    std::unique_lock lock(mu_);
    if (!IsValidUrl(u) || seen_.Contains(u)) {
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
        if (!IsValidUrl(u) || seen_.Contains(u)) {
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
