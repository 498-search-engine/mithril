#ifndef CRAWLER_CRAWLERMETRICS_H
#define CRAWLER_CRAWLERMETRICS_H

#include "metrics/Metrics.h"
#include "metrics/MetricsServer.h"

namespace mithril {

using namespace metrics;

inline auto DocumentsProcessedMetric = Metric{
    "crawler_documents_processed",
    "counter",
    "Number of documents processed by the crawler",
};

inline auto CrawlResponseCodesMetric = Metric{
    "crawler_crawl_response_codes",
    "counter",
    "Number of crawl responses with a HTTP status code",
};

inline auto RobotsResponseCodesMetric = Metric{
    "crawler_robots_response_codes",
    "counter",
    "Number of robots.txt responses with a HTTP status code",
};

inline auto InFlightCrawlRequestsMetric = Metric{
    "crawler_in_flight_crawl_requests",
    "gauge",
    "Number of actively-executing crawl requests",
};

inline auto InFlightRobotsRequestsMetric = Metric{
    "crawler_in_flight_robots_requests",
    "gauge",
    "Number of actively-executing robots.txt requests",
};

inline auto WaitingRobotsHosts = Metric{
    "crawler_waiting_robots_hosts",
    "gauge",
    "Number of hosts waiting for robots.txt to be resolved",
};

inline auto WaitingRobotsURLs = Metric{
    "crawler_waiting_robots_urls",
    "gauge",
    "Number of URLs waiting for robots.txt to be resolved for their hosts",
};

inline auto DocumentQueueSizeMetric = Metric{
    "crawler_document_queue_size",
    "gauge",
    "Number of documents in document queue waiting to be processed by a worker",
};

inline auto MiddleQueueActiveQueueCount = Metric{
    "crawler_middle_queue_active_queue_count",
    "gauge",
    "Number of active, in-use queues within the middle queue",
};

inline auto MiddleQueueTotalQueuedURLs = Metric{
    "crawler_middle_queue_total_queued_urls",
    "gauge",
    "Number of queued URLs across all queues in the middle queue",
};

inline auto FrontierSize = Metric{
    "crawler_frontier_size",
    "gauge",
    "Number of URLs on the frontier, crawled or not yet crawled",
};

inline auto FrontierQueueSize = Metric{
    "crawler_frontier_queue_size",
    "gauge",
    "Number of URLs on the frontier yet to be crawled",
};

inline auto FrontierFreshURLs = Metric{
    "crawler_frontier_fresh_urls",
    "gauge",
    "Number of fresh URLs waiting to be pushed onto the frontier",
};

inline auto RegisterCrawlerMetrics(MetricsServer& server) {
    server.Register(&DocumentsProcessedMetric);
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
