#include "UrlSet.h"

#include "http/URL.h"

#include <iostream>
#include <string>
#include <utility>

namespace mithril {

void UrlSet::Put(const std::string& url) {
    if (auto parsed = http::ParseURL(url)) {
        auto normalized = http::CanonicalizeURL(*parsed);
        set_.insert(std::move(normalized));
    } else {
        std::cerr << "bad url: " << url << std::endl;
    }
}

bool UrlSet::Contains(const std::string& url) const {
    if (auto parsed = http::ParseURL(url)) {
        auto normalized = http::CanonicalizeURL(*parsed);
        return set_.contains(normalized);
    }
    return false;
}

}  // namespace mithril
