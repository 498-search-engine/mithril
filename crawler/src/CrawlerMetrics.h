#ifndef CRAWLER_CRAWLERMETRICS_H
#define CRAWLER_CRAWLERMETRICS_H

#include "metrics/Metrics.h"
#include "metrics/MetricsServer.h"

namespace mithril {

using namespace metrics;

inline auto TotalDocumentCorpusSizeMetric = Metric{
    "crawler_document_corpus_size",
    MetricTypeGauge,
    "Number of documents in the document corpus",
};

inline auto DocumentsProcessedMetric = Metric{
    "crawler_documents_processed",
    MetricTypeCounter,
    "Number of documents processed by the crawler",
};

inline auto DocumentProcessDurationMetric = HistogramMetric{
    "crawler_document_process_duration",
    "Document process duration in seconds",
    ExponentialBuckets(0.001, 2, 11),
};

inline auto DocumentSizeBytesMetric = HistogramMetric{
    "crawler_document_size_bytes",
    "Processed document size in bytes",
    {1 << 10, 1 << 12, 1 << 14, 1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22},
};

inline auto CrawlRequestErrorsMetric = Metric{
    "crawler_crawl_request_errors",
    MetricTypeCounter,
    "Number of errors encountered during a crawl request",
};

inline auto CrawlResponseCodesMetric = Metric{
    "crawler_crawl_response_codes",
    MetricTypeCounter,
    "Number of crawl responses with a HTTP status code",
};

inline auto RobotsResponseCodesMetric = Metric{
    "crawler_robots_response_codes",
    MetricTypeCounter,
    "Number of robots.txt responses with a HTTP status code",
};

inline auto InFlightCrawlRequestsMetric = Metric{
    "crawler_in_flight_crawl_requests",
    MetricTypeGauge,
    "Number of actively-executing crawl requests",
};

inline auto InFlightRobotsRequestsMetric = Metric{
    "crawler_in_flight_robots_requests",
    MetricTypeGauge,
    "Number of actively-executing robots.txt requests",
};

inline auto WaitingRobotsHosts = Metric{
    "crawler_waiting_robots_hosts",
    MetricTypeGauge,
    "Number of hosts waiting for robots.txt to be resolved",
};

inline auto WaitingRobotsURLs = Metric{
    "crawler_waiting_robots_urls",
    MetricTypeGauge,
    "Number of URLs waiting for robots.txt to be resolved for their hosts",
};

inline auto RobotRulesCacheQueuedFetchesCount = Metric{
    "crawler_robot_rules_cache_queued_fetches_count",
    MetricTypeGauge,
    "Number of queued robots fetches in the robot rules cache",
};

inline auto RobotRulesCacheQueuedFetchesWaitingCount = Metric{
    "crawler_robot_rules_cache_queued_fetches_waiting_count",
    MetricTypeGauge,
    "Number of queued robots fetches in the robot rules cache that are currently rate limited",
};

inline auto RobotRulesCacheHits = Metric{
    "crawler_robot_rules_cache_hits",
    MetricTypeCounter,
    "Number of cache hits for robot rules lookup",
};

inline auto RobotRulesCacheMisses = Metric{
    "crawler_robot_rules_cache_misses",
    MetricTypeCounter,
    "Number of cache misses for robot rules lookup",
};

inline auto DocumentQueueSizeMetric = Metric{
    "crawler_document_queue_size",
    MetricTypeGauge,
    "Number of documents in document queue waiting to be processed by a worker",
};

inline auto MiddleQueueTotalQueuedURLs = Metric{
    "crawler_middle_queue_total_queued_urls",
    MetricTypeGauge,
    "Number of queued URLs across all queues in the middle queue",
};

inline auto MiddleQueueTotalQueues = Metric{
    "crawler_middle_queue_total_queues",
    MetricTypeGauge,
    "Number of configured total queues in the middle queue",
};

inline auto MiddleQueueActiveQueueCount = Metric{
    "crawler_middle_queue_active_count",
    MetricTypeGauge,
    "Number of active, in-use queues within the middle queue",
};

inline auto MiddleQueueTotalHosts = Metric{
    "crawler_middle_queue_total_hosts",
    MetricTypeGauge,
    "Number of tracked hosts in the middle queue",
};

inline auto MiddleQueueHostCooldownCount = Metric{
    "crawler_middle_queue_host_cooldown_count",
    MetricTypeGauge,
    "Number of active hosts in middle queue that are currently under cooldown",
};

inline auto MiddleQueueRateLimitedCount = Metric{
    "crawler_middle_queue_rate_limited_count",
    MetricTypeGauge,
    "Number of active hosts in middle queue that are currently rate limited",
};

inline auto MiddleQueueWaitingDelayLookupCount = Metric{
    "crawler_middle_queue_waiting_delay_lookup_count",
    MetricTypeGauge,
    "Number of active hosts in middle queue that are waiting for a delay lookup",
};

inline auto FrontierSize = Metric{
    "crawler_frontier_size",
    MetricTypeGauge,
    "Number of URLs on the frontier, crawled or not yet crawled",
};

inline auto FrontierQueueSize = Metric{
    "crawler_frontier_queue_size",
    MetricTypeGauge,
    "Number of URLs on the frontier yet to be crawled",
};

inline auto FrontierFreshURLs = Metric{
    "crawler_frontier_fresh_urls",
    MetricTypeGauge,
    "Number of fresh URLs waiting to be pushed onto the frontier",
};

inline auto CrawlDelayLookupLockFailures = Metric{
    "crawler_delay_lookup_lock_failures",
    MetricTypeCounter,
    "Number of times acquiring the robots cache lock failed when looking up crawl delay",
};

inline auto CrawlDelayLookupLockSuccesses = Metric{
    "crawler_delay_lookup_lock_successes",
    MetricTypeCounter,
    "Number of times acquiring the robots cache lock succeeded when looking up crawl delay",
};

inline auto ProcessFreshURLsCounter = Metric{
    "crawler_process_fresh_urls_counter",
    MetricTypeCounter,
    "Number of times fresh URLs were processed",
};

inline auto ProcessRobotsRequestsCounter = Metric{
    "crawler_process_robots_requests_counter",
    MetricTypeCounter,
    "Number of times robots requests were processed",
};

inline auto RegisterCrawlerMetrics(MetricsServer& server) {
    server.Register(&TotalDocumentCorpusSizeMetric);
    server.Register(&DocumentsProcessedMetric);
    server.Register(&DocumentProcessDurationMetric);
    server.Register(&DocumentSizeBytesMetric);
    server.Register(&CrawlRequestErrorsMetric);
    server.Register(&CrawlResponseCodesMetric);
    server.Register(&RobotsResponseCodesMetric);
    server.Register(&InFlightCrawlRequestsMetric);
    server.Register(&InFlightRobotsRequestsMetric);
    server.Register(&WaitingRobotsHosts);
    server.Register(&WaitingRobotsURLs);
    server.Register(&RobotRulesCacheQueuedFetchesCount);
    server.Register(&RobotRulesCacheQueuedFetchesWaitingCount);
    server.Register(&RobotRulesCacheHits);
    server.Register(&RobotRulesCacheMisses);
    server.Register(&DocumentQueueSizeMetric);
    server.Register(&MiddleQueueTotalQueuedURLs);
    server.Register(&MiddleQueueTotalQueues);
    server.Register(&MiddleQueueActiveQueueCount);
    server.Register(&MiddleQueueTotalHosts);
    server.Register(&MiddleQueueHostCooldownCount);
    server.Register(&MiddleQueueRateLimitedCount);
    server.Register(&MiddleQueueWaitingDelayLookupCount);
    server.Register(&FrontierSize);
    server.Register(&FrontierQueueSize);
    server.Register(&FrontierFreshURLs);
    server.Register(&CrawlDelayLookupLockFailures);
    server.Register(&CrawlDelayLookupLockSuccesses);
    server.Register(&ProcessFreshURLsCounter);
    server.Register(&ProcessRobotsRequestsCounter);
}

}  // namespace mithril

#endif
