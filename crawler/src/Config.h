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

    std::string frontierDirectory = "data/frontier";
};

CrawlerConfig LoadConfigFromFile(const std::string& path);

}  // namespace mithril

#endif
