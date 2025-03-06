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

inline auto DocumentQueueSizeMetric = Metric{
    "crawler_document_queue_size",
    "gauge",
    "Number of documents in document queue waiting to be processed by a worker",
};

inline auto RegisterCrawlerMetrics(MetricsServer& server) {
    server.Register(&DocumentsProcessedMetric);
    server.Register(&CrawlResponseCodesMetric);
    server.Register(&RobotsResponseCodesMetric);
    server.Register(&InFlightCrawlRequestsMetric);
    server.Register(&InFlightRobotsRequestsMetric);
    server.Register(&DocumentQueueSizeMetric);
}

}  // namespace mithril

#endif
