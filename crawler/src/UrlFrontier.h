#ifndef CRAWLER_URLFRONTIER_H
#define CRAWLER_URLFRONTIER_H

#include "UrlSet.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <core/dary_heap.h>

namespace mithril {

class UrlFrontier {
public:
    UrlFrontier();

    bool Empty() const;

    /**
     * @brief Gets at least one URL from the frontier, up to max
     *
     * @param max Max URLs to get
     * @param out Output vector to put URLs into
     * @param atLeastOne Wait for at least one URL
     */
    void GetURLs(size_t max, std::vector<std::string>& out, bool atLeastOne = false);

    /**
     * @brief Puts a url onto the frontier (if not already visited).
     *
     * @param u URL to add to frontier.
     * @return 1 if accepted, 0 if not
     */
    int PutURL(std::string u);

    /**
     * @brief Puts multiple urls onto the frontier (if not already visited)
     *
     * @param urls URLs to add to frontier.
     * @return Number of accepted URLs.
     */
    int PutURLs(std::vector<std::string> urls);

private:
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;

    std::queue<std::string> urls_;
    UrlSet seen_;
    core::dary_heap<std::string, int> test_heap;
};

}  // namespace mithril

#endif
