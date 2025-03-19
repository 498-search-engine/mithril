#include "Config.h"

#include "FileSystem.h"
#include "Util.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mithril {

using namespace std::string_view_literals;

namespace {

std::string Trim(std::string_view str) {
    size_t first = str.find_first_not_of(" \t"sv);
    size_t last = str.find_last_not_of(" \t"sv);
    return std::string(str.substr(first, last - first + 1));
}

}  // namespace

CrawlerConfig LoadConfigFromFile(const std::string& path) {
    CrawlerConfig config;

    auto fileDir = Dirname(path.c_str());
    auto fileData = ReadFile(path.c_str());
    auto lines = GetLines(fileData);

    size_t lineNumber = 0;
    for (auto line : lines) {
        ++lineNumber;
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string_view::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(lineNumber) + ": missing '='");
        }

        auto key = Trim(line.substr(0, eqPos));
        auto value = Trim(line.substr(eqPos + 1));

        if (key == "workers"sv) {
            config.num_workers = std::stoul(std::string(value));
            if (config.num_workers == 0) {
                throw std::runtime_error("workers must be > 0");
            }
        } else if (key == "concurrent_requests"sv) {
            config.concurrent_requests = std::stoul(std::string(value));
            if (config.concurrent_requests == 0) {
                throw std::runtime_error("concurrent_requests must be > 0");
            }
        } else if (key == "seed_url"sv) {
            if (!value.empty()) {
                config.seed_urls.emplace_back(value);
            }
        } else if (key == "blacklist_host"sv) {
            if (!value.empty()) {
                config.blacklist_hosts.insert(std::string{value});
            }
        } else if (key == "request_timeout"sv) {
            config.request_timeout = std::stoul(std::string(value));
        } else if (key == "data_directory"sv) {
            config.data_directory = value;
        } else if (key == "default_crawl_delay_ms"sv) {
            config.default_crawl_delay_ms = std::stol(std::string(value));
        } else if (key == "middle_queue.queue_count"sv) {
            config.middle_queue_queue_count = std::stoul(std::string(value));
        } else if (key == "middle_queue.url_batch_size"sv) {
            config.middle_queue_url_batch_size = std::stoul(std::string(value));
        } else if (key == "middle_queue.host_url_limit"sv) {
            config.middle_queue_host_url_limit = std::stoul(std::string(value));
        } else if (key == "middle_queue.utilization_target"sv) {
            config.middle_queue_utilization_target = std::stod(std::string(value));
        } else if (key == "concurrent_robots_requests"sv) {
            config.concurrent_robots_requests = std::stoul(std::string(value));
        } else if (key == "metrics_port"sv) {
            config.metrics_port = static_cast<uint16_t>(std::stoul(std::string(value)));
        } else if (key == "snapshot_period_seconds"sv) {
            config.snapshot_period_seconds = std::stoul(std::string(value));
        } else if (key == "seed_url_file"sv) {
            auto path = fileDir + "/" + std::string{value};
            auto f = ReadFile(path.c_str());
            auto lines = GetLines(f);
            for (auto line : lines) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                config.seed_urls.emplace_back(line);
            }
        } else if (key == "blacklist_host_file"sv) {
            auto path = fileDir + "/" + std::string{value};
            auto f = ReadFile(path.c_str());
            auto lines = GetLines(f);
            for (auto line : lines) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                config.blacklist_hosts.insert(std::string{line});
            }
        }
    }

    if (config.seed_urls.empty()) {
        throw std::runtime_error("No seed URLs configured");
    }

    return config;
}

}  // namespace mithril
