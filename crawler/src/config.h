#ifndef CRAWLER_CONFIG_H
#define CRAWLER_CONFIG_H

#include <string>
#include <vector>

namespace mithril {

struct CrawlerConfig {
    size_t num_workers = 2;
    size_t concurrent_requests = 10;
    std::vector<std::string> seed_urls;

    static CrawlerConfig FromFile(const std::string& path);
};

} // namespace mithril

#endif