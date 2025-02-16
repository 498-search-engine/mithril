#ifndef CRAWLER_URLFRONTIER_H
#define CRAWLER_URLFRONTIER_H

#include "Robots.h"
#include "UrlSet.h"
#include "http/URL.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace mithril {

class UrlFrontier {
public:
    UrlFrontier() = default;

    /**
     * @brief Processes in-flight and pending robots.txt requests for URLs
     * waiting to get into the frontier.
     */
    void ProcessRobotsRequests();

    /**
     * @brief Processes freshly-added URLs from PushURL and PushURLs,
     * determining whether each URL is valid, has been seen before, and is
     * allowed by robots.txt before pushing the URL onto the frontier.
     */
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
     * @brief Pushes a url onto the frontier (if not already visited).
     *
     * @param u URL to add to frontier.
     */
    void PushURL(std::string u);

    /**
     * @brief Pushes multiple urls onto the frontier (if not already visited).
     *
     * @param urls URLs to add to frontier.
     */
    void PushURLs(std::vector<std::string>& urls);

private:
    /**
     * @brief Pushes the URL, which has been approved to enter the frontier,
     * onto the actual frontier structure. Requires any necessary locks to be
     * held by the caller.
     *
     * @param url URL to push into the frontier.
     */
    void PushAcceptedURL(std::string url);

    mutable std::mutex urlQueueMu_;     // Lock for urls_
    mutable std::mutex seenMu_;         // Lock for seen_
    mutable std::mutex robotsCacheMu_;  // Lock for robotRulesCache_
    mutable std::mutex waitingUrlsMu_;  // Lock for urlsWaitingForRobots_
    mutable std::mutex freshURLsMu_;    // Lock for freshURLs_

    std::condition_variable urlQueueCv_;   // Notifies when URL is available in queue
    std::condition_variable robotsCv_;     // Notifies when new request is available for processing
    std::condition_variable freshURLsCv_;  // Notifies when a fresh URL is available for processing

    // Queue of validated and allowed URLs for us to crawl
    std::queue<std::string> urls_;
    // Set of visited/currently-processing URLs, including those we chose not to
    // crawl
    UrlSet seen_;

    // Cache for robots.txt rulesets
    RobotRulesCache robotRulesCache_;
    // URLs waiting for a robots.txt request to complete
    std::unordered_map<http::CanonicalHost, std::vector<http::URL>> urlsWaitingForRobots_;

    // List of fresh URLs to consider for placement into the frontier, pushed by
    // workers
    std::vector<std::string> freshURLs_;
};

}  // namespace mithril

#endif
