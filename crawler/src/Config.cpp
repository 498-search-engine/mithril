#include "Config.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mithril {

namespace {

std::string Trim(std::string_view str) {
    size_t first = str.find_first_not_of(" \t");
    size_t last = str.find_last_not_of(" \t");
    return std::string(str.substr(first, last - first + 1));
}

}  // namespace

CrawlerConfig LoadConfigFromFile(const std::string& path) {
    CrawlerConfig config;
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(file, line)) {
        lineNumber++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(lineNumber) + ": missing '='");
        }

        auto key = Trim(std::string_view(line.data(), eqPos));
        auto value = Trim(std::string_view(line.data() + eqPos + 1));

        if (key == "workers") {
            config.num_workers = std::stoul(std::string(value));
            if (config.num_workers == 0) {
                throw std::runtime_error("workers must be > 0");
            }
        } else if (key == "concurrent_requests") {
            config.concurrent_requests = std::stoul(std::string(value));
            if (config.concurrent_requests == 0) {
                throw std::runtime_error("concurrent_requests must be > 0");
            }
        } else if (key == "seed_url") {
            if (!value.empty()) {
                config.seed_urls.push_back(std::string(value));
            }
        } else if (key == "request_timeout") {
            config.request_timeout = std::stoul(std::string(value));
        } else if (key == "data_directory") {
            config.data_directory = value;
        } else if (key == "default_crawl_delay_ms") {
            config.default_crawl_delay_ms = std::stol(std::string(value));
        } else if (key == "middle_queue.queue_count") {
            config.middle_queue_queue_count = std::stoul(std::string(value));
        } else if (key == "middle_queue.url_batch_size") {
            config.middle_queue_url_batch_size = std::stoul(std::string(value));
        } else if (key == "middle_queue.host_url_limit") {
            config.middle_queue_host_url_limit = std::stoul(std::string(value));
        } else if (key == "middle_queue.utilization_target") {
            config.middle_queue_utilization_target = std::stod(std::string(value));
        } else if (key == "concurrent_robots_requests") {
            config.concurrent_robots_requests = std::stoul(std::string(value));
        } else if (key == "metrics_port") {
            config.metrics_port = static_cast<uint16_t>(std::stoul(std::string(value)));
        } else if (key == "snapshot_period_seconds") {
            config.snapshot_period_seconds = std::stoul(std::string(value));
        }
    }

    if (config.seed_urls.empty()) {
        throw std::runtime_error("No seed URLs configured");
    }

    return config;
}

}  // namespace mithril
