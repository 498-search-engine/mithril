#ifndef CRAWLER_CONFIG_H
#define CRAWLER_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mithril {

struct CrawlerConfig {
    std::string log_level = "info";

    std::string docs_directory;
    std::string state_directory;
    std::string snapshot_directory;

    unsigned int frontier_growth_rate_bp = 10000;

    size_t dns_cache_size = 100000;

    size_t num_workers = 2;
    size_t concurrent_requests = 10;
    unsigned long request_timeout = 10;  // seconds

    std::vector<std::string> seed_urls;
    std::set<std::string> blacklist_hosts;

    long default_crawl_delay_ms = 200;  // milliseconds
    long ratelimit_bucket_ms = 60000;
    unsigned int ratelimit_bucket_count = 60;

    size_t middle_queue_queue_count = 100;
    size_t middle_queue_url_batch_size = 10;
    size_t middle_queue_host_url_limit = 25;
    double middle_queue_utilization_target = 0.25;

    size_t concurrent_robots_requests = 100;
    size_t robots_cache_size = 50000;

    uint16_t metrics_port = 9000;
    unsigned long snapshot_period_seconds = 30L * 60L;
};

CrawlerConfig LoadConfigFromFile(const std::string& path);

}  // namespace mithril

#endif
