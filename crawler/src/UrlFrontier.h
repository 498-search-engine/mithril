#ifndef CRAWLER_URLFRONTIER_H
#define CRAWLER_URLFRONTIER_H

#include "Robots.h"
#include "UrlSet.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

namespace mithril {

class UrlFrontier {
public:
    UrlFrontier();

    bool Empty() const;

    void ProcessRobotsRequests();

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
     */
    void PutURL(std::string u);

    /**
     * @brief Puts multiple urls onto the frontier (if not already visited)
     *
     * @param urls URLs to add to frontier.
     */
    void PutURLs(std::vector<std::string> urls);

private:
    struct WaitingURLs {
        http::URL canonicalHost;
        std::vector<http::URL> urls;
    };

    bool PutURLInternal(std::string u);

    mutable std::mutex mu_;
    mutable std::condition_variable cv_;
    mutable std::condition_variable robotsCv_;

    std::queue<std::string> urls_;
    UrlSet seen_;

    RobotRulesCache robotRulesCache_;
    std::unordered_map<std::string, WaitingURLs> urlsWaitingForRobots_;
};

}  // namespace mithril

#endif
