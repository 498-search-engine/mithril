#ifndef CRAWLER_URLFRONTIER_H
#define CRAWLER_URLFRONTIER_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>
#include <core/dary_heap.h>

namespace mithril {

class UrlFrontier {
public:
    UrlFrontier();

    bool Empty() const;

    std::optional<std::string> GetURL();
    std::vector<std::string> GetURLs(size_t max);
    void PutURL(std::string u);

private:
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;

    std::queue<std::string> urls_;
    std::unordered_set<std::string> seen_;
    core::dary_heap<std::string, int> test_heap; 
};

}  // namespace mithril

#endif
