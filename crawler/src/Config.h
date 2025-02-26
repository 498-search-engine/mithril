#ifndef CRAWLER_CONFIG_H
#define CRAWLER_CONFIG_H

#include <cstddef>
#include <string>
#include <vector>

namespace mithril {

struct CrawlerConfig {
    size_t num_workers = 2;
    size_t concurrent_requests = 10;
    unsigned long request_timeout = 10;  // seconds
    std::vector<std::string> seed_urls;

    std::string data_directory = "data";

    long default_crawl_delay_ms = 200;  // milliseconds
    size_t middle_queue_queue_count = 100;
    size_t middle_queue_url_batch_size = 10;
    size_t middle_queue_host_url_limit = 25;
    double middle_queue_utilization_target = 0.25;
};

CrawlerConfig LoadConfigFromFile(const std::string& path);

}  // namespace mithril

#endif
