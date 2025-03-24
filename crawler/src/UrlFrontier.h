#ifndef CRAWLER_URLFRONTIER_H
#define CRAWLER_URLFRONTIER_H

#include "PriorityURLQueue.h"
#include "Robots.h"
#include "ThreadSync.h"
#include "core/cv.h"
#include "core/locks.h"
#include "core/mutex.h"
#include "core/optional.h"
#include "http/URL.h"
#include "ranking/CrawlerRanker.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mithril {

class UrlFrontier {
public:
    UrlFrontier(const std::string& frontierDirectory, size_t concurrentRobotsRequests, size_t robotsCacheSize);

    /**
     * @brief Initializes notifications on cv instances for ThreadSync.
     *
     * @param sync ThreadSync instance to be used for operations.
     */
    void InitSync(ThreadSync& sync);

    /**
     * @brief Returns the total size of the frontier, i.e. the number of
     * previously visited documents plus the number of planned-to-visit
     * documents.
     */
    size_t TotalSize() const;

    /**
     * @brief Returns whether the frontier is empty in terms of fresh documents
     * to process.
     */
    bool Empty() const;

    /**
     * @brief Copies the URL frontier state into the given directory.
     *
     * @param directory Directory to copy into.
     * @return Whether the operation succeeded.
     */
    bool CopyStateToDirectory(const std::string& directory) const;

    /**
     * @brief Runs the robots requests processing thread until an indicated
     * shutdown.
     *
     * @param sync ThreadSync to communicate shutdown.
     */
    void RobotsRequestsThread(ThreadSync& sync);

    /**
     * @brief Runs the fresh url processing thread until an indicated shutdown.
     *
     * @param sync ThreadSync to communicate shutdown.
     */
    void FreshURLsThread(ThreadSync& sync);

    /**
     * @brief Resets the timeout progress for all active robots requests.
     */
    void TouchRobotRequestTimeouts();

    /**
     * @brief Look up the Crawl-Delay directive for a host. Obtains the
     * specified value, a default if the host does not specify a default, and
     * core::nullopt if the lookup is pending. Does not block.
     *
     * @param host Host to look up
     * @param defaultDelay Default delay if host does not specify
     */
    core::Optional<unsigned long> LookUpCrawlDelayNonblocking(const http::CanonicalHost& host,
                                                              unsigned long defaultDelay);

    /**
     * @brief Gets at least one URL from the frontier, up to max
     *
     * @param sync ThreadSync to cancel/pause waiting operations
     * @param max Max URLs to get
     * @param out Output vector to put URLs into
     * @param atLeastOne Wait for at least one URL
     */
    void GetURLs(ThreadSync& sync, size_t max, std::vector<std::string>& out, bool atLeastOne = false);

    /**
     * @brief Gets at least one URL from the frontier, up to max, filtered.
     *
     * @param sync ThreadSync to cancel/pause waiting operations
     * @param max Max URLs to get
     * @param out Output vector to put URLs into
     * @param f Filter predicate for URLs
     * @param atLeastOne Wait for at least one URL
     */
    template<typename Filter>
    void
    GetURLsFiltered(ThreadSync& sync, size_t max, std::vector<std::string>& out, Filter f, bool atLeastOne = false) {
        if (max == 0) {
            return;
        }

        core::LockGuard lock(urlQueueMu_, core::DeferLock);
        if (atLeastOne) {
            // The caller wants at least one URL, we need to wait until we have at
            // least one.
            lock.Lock();
            urlQueueCv_.Wait(lock, [&]() { return !urlQueue_.Empty() || sync.ShouldSynchronize(); });
        } else {
            // The caller doesn't care if we can't get a URL. If we can't grab the
            // lock right now, we won't wait around.
            if (!lock.TryLock()) {
                return;
            }
            if (urlQueue_.Empty()) {
                return;
            }
        }

        if (sync.ShouldSynchronize() || urlQueue_.Empty()) {
            return;
        }
        urlQueue_.PopURLs(max, out, f);
    }

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

    void DumpPendingURLs(std::vector<std::string>& urls);

private:
    /**
     * @brief Processes in-flight and pending robots.txt requests for URLs
     * waiting to get into the frontier.
     */
    void ProcessRobotsRequests(ThreadSync& sync);

    /**
     * @brief Processes freshly-added URLs from PushURL and PushURLs,
     * determining whether each URL is valid, has been seen before, and is
     * allowed by robots.txt before pushing the URL onto the frontier.
     */
    void ProcessFreshURLs(ThreadSync& sync);

    struct Scorer {
        // TODO: accept string_view instead?
        static unsigned int Score(std::string_view url) { return ranking::GetUrlRank(std::string{url}); }
    };

    mutable core::Mutex urlQueueMu_;     // Lock for urls_
    mutable core::Mutex robotsCacheMu_;  // Lock for robotRulesCache_
    mutable core::Mutex waitingUrlsMu_;  // Lock for urlsWaitingForRobots_
    mutable core::Mutex freshURLsMu_;    // Lock for freshURLs_

    core::cv urlQueueCv_;   // Notifies when URL is available in queue
    core::cv robotsCv_;     // Notifies when new request is available for processing
    core::cv freshURLsCv_;  // Notifies when a fresh URL is available for processing

    PriorityURLQueue<Scorer> urlQueue_;

    // Cache for robots.txt rulesets
    RobotRulesCache robotRulesCache_;
    // URLs waiting for a robots.txt request to complete
    std::unordered_map<http::CanonicalHost, std::vector<http::URL>> urlsWaitingForRobots_;
    size_t urlsWaitingForRobotsCount_{0};

    // List of fresh URLs to consider for placement into the frontier, pushed by
    // workers
    std::vector<std::string> freshURLs_;
};

}  // namespace mithril

#endif
