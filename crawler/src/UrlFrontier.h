#ifndef CRAWLER_URLFRONTIER_H
#define CRAWLER_URLFRONTIER_H

#include "Robots.h"
#include "UrlSet.h"
#include "http/URL.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace mithril {

class UrlFrontier {
public:
    UrlFrontier();

    bool Empty() const;

    void ProcessRobotsRequests();

    void ProcessFreshURLs();

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
    void PutURLs(std::vector<std::string>& urls);

private:
    mutable std::mutex urlQueueMu_;     // Lock for urls_
    mutable std::mutex seenMu_;         // Lock for seen_
    mutable std::mutex robotsCacheMu_;  // Lock for robotRulesCache_
    mutable std::mutex waitingUrlsMu_;  // Lock for urlsWaitingForRobots_
    mutable std::mutex freshURLsMu_;    // Lock for freshURLs_

    std::condition_variable urlQueueCv_;   // Notifies when URL is available in queue
    std::condition_variable robotsCv_;     // Notifies when new request is available for processing
    std::condition_variable freshURLsCv_;  // Notifies when fresh URL added

    std::queue<std::string> urls_;
    UrlSet seen_;

    RobotRulesCache robotRulesCache_;
    std::unordered_map<http::CanonicalHost, std::vector<http::URL>> urlsWaitingForRobots_;

    std::vector<std::string> freshURLs_;
};

}  // namespace mithril

#endif
