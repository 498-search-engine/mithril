#ifndef CRAWLER_MIDDLEQUEUE_H
#define CRAWLER_MIDDLEQUEUE_H


#include "Config.h"
#include "HostRateLimiter.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "core/memory.h"
#include "core/optional.h"
#include "http/URL.h"

#include <cstddef>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mithril {

class MiddleQueue {
public:
    MiddleQueue(UrlFrontier* frontier, HostRateLimiter* limiter, const CrawlerConfig& config);

    MiddleQueue(UrlFrontier* frontier,
                HostRateLimiter* limiter,
                size_t numQueues,
                size_t urlBatchSize,
                size_t hostUrlLimit,
                double queueUtilizationTarget,
                unsigned long defaultCrawlDelayMs);

    /**
     * @brief Gets URLs from the middle queue, pulling from the frontier if
     * necessary.
     *
     * @param sync ThreadSync to cancel/pause waiting operations
     * @param max Max URLs to get
     * @param out Output vector to put URLs into
     * @param atLeastOne Try to wait for at least one URL
     */
    void GetURLs(ThreadSync& sync, size_t max, std::vector<std::string>& out, bool atLeastOne = false);

    /**
     * @brief Restores the middle queue state from a vector of URLs.
     *
     * @param urls URLs to populate middle queue with.
     */
    void RestoreFrom(std::vector<std::string>& urls);

    /**
     * @brief Extracts the middle queue state into a vector of URLs.
     *
     * @param out Vector to put URLs into.
     */
    void DumpQueuedURLs(std::vector<std::string>& out);

private:
    struct HostRecord {
        http::CanonicalHost host;
        bool waitingDelayLookup{true};
        std::queue<std::string> queue;
        core::Optional<size_t> activeQueue;
    };

    /**
     * @brief Compute a safe, reasonable crawl delay in milliseconds from a
     * Crawl-Delay directive.
     *
     * @param directive Crawl-Delay directive value (seconds)
     */
    unsigned long CrawlDelayFromDirective(unsigned long directive) const;

    /**
     * @brief Returns the number of queues actively in use.
     */
    size_t ActiveQueueCount() const;

    /**
     * @brief Returns the percentage of queues actively in use.
     */
    double QueueUtilization() const;

    /**
     * @brief Add a URL into the middle queue.
     *
     * @param now Current timestamp (milliseconds)
     * @param url URL to add
     */
    void AcceptURL(long now, std::string url);

    /**
     * @brief Adds a URL to a HostRecord
     *
     * @param url URL to add
     * @param record HostRecord to add to
     */
    void PushURLForHost(std::string url, HostRecord* record);

    /**
     * @brief Adds a URL for a host, creating the associated HostRecord.
     *
     * @param url URL to add
     * @param host Host of URL
     */
    void PushURLForNewHost(std::string url, const http::CanonicalHost& host);

    /**
     * @brief Pops a URL from the queue of a host. Requires the host to have a
     * URL in its queue. If the host's queue becomes empty, it is removed from
     * the active queues set and is replaced if possible.
     *
     * @param record Host to pop from
     * @return std::string Popped URL
     */
    std::string PopFromHost(HostRecord& record);

    /**
     * @brief Checks for hosts with waiting URLs and adds them to the active
     * queue set.
     */
    void PopulateActiveQueues();

    /**
     * @brief Assigns a host record to a free queue. Requires there to actually
     * be a free queue.
     *
     * @param record Host to assign a queue.
     */
    void AssignFreeQueue(HostRecord* record);

    /**
     * @brief Cleans out the internal mapping of hosts that don't have any
     * queued URLs.
     */
    void CleanEmptyHosts();

    /**
     * @brief Checks whether we want to accept a URL into the middle queue at
     * the moment. We may reject a URL if its associated host already has a lot
     * of waiting URLs.
     *
     * @param url URL to check
     */
    bool WantURL(std::string_view url) const;

    UrlFrontier* frontier_;
    HostRateLimiter* limiter_;
    size_t n_;
    size_t urlBatchSize_;
    size_t hostUrlLimit_;
    double queueUtilizationTarget_;
    unsigned long defaultCrawlDelayMs_;

    size_t k_{0};
    size_t totalQueuedURLs_{0};

    std::unordered_map<std::string, core::UniquePtr<HostRecord>> hosts_;
    std::vector<HostRecord*> queues_;
    std::vector<size_t> emptyQueues_;
};

}  // namespace mithril

#endif
