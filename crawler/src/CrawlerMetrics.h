#ifndef CRAWLER_CRAWLERMETRICS_H
#define CRAWLER_CRAWLERMETRICS_H

#include "metrics/Metrics.h"
#include "metrics/MetricsServer.h"

namespace mithril {

using namespace metrics;

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

inline auto DocumentQueueSizeMetric = Metric{
    "crawler_document_queue_size",
    MetricTypeGauge,
    "Number of documents in document queue waiting to be processed by a worker",
};

inline auto MiddleQueueActiveQueueCount = Metric{
    "crawler_middle_queue_active_queue_count",
    MetricTypeGauge,
    "Number of active, in-use queues within the middle queue",
};

inline auto MiddleQueueTotalQueuedURLs = Metric{
    "crawler_middle_queue_total_queued_urls",
    MetricTypeGauge,
    "Number of queued URLs across all queues in the middle queue",
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

inline auto RegisterCrawlerMetrics(MetricsServer& server) {
    server.Register(&DocumentsProcessedMetric);
    server.Register(&DocumentProcessDurationMetric);
    server.Register(&DocumentSizeBytesMetric);
    server.Register(&CrawlResponseCodesMetric);
    server.Register(&RobotsResponseCodesMetric);
    server.Register(&InFlightCrawlRequestsMetric);
    server.Register(&InFlightRobotsRequestsMetric);
    server.Register(&WaitingRobotsHosts);
    server.Register(&WaitingRobotsURLs);
    server.Register(&DocumentQueueSizeMetric);
    server.Register(&MiddleQueueActiveQueueCount);
    server.Register(&MiddleQueueTotalQueuedURLs);
    server.Register(&FrontierSize);
    server.Register(&FrontierQueueSize);
    server.Register(&FrontierFreshURLs);
}

}  // namespace mithril

#endif
