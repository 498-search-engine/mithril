#ifndef CRAWLER_URLSET_H
#define CRAWLER_URLSET_H

#include <string>
#include <unordered_set>

#include "http/URL.h"

namespace mithril {

/**
 * @brief An insert-only set of URLs.
 */
class UrlSet {
public:
    void Put(const std::string& url);
    bool Contains(const std::string& url) const;

private:
    std::unordered_set<std::string> set_;
};

}  // namespace mithril

#endif
