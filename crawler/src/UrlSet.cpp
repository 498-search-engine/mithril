#include "UrlSet.h"

#include <string>

namespace mithril {

void UrlSet::Put(const std::string& url) {
    set_.insert(url);
}

bool UrlSet::Contains(const std::string& url) const {
    return set_.contains(url);
}

}  // namespace mithril
