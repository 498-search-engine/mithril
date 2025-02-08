#include "UrlSet.h"

#include <string>

namespace mithril {

void UrlSet::Put(const std::string& url) {
    auto parsed = http::ParseURL(url);
    auto normalized = http::NormalizeURL(parsed);
    set_.insert(std::move(normalized));
}

bool UrlSet::Contains(const std::string& url) const {
    auto parsed = http::ParseURL(url);
    auto normalized = http::NormalizeURL(parsed);
    return set_.contains(normalized);
}

}  // namespace mithril
