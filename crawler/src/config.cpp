#include "config.h"

#include <fstream>
#include <stdexcept>
#include <string>

namespace mithril {

namespace {
std::string trim(std::string_view str) {
    size_t first = str.find_first_not_of(" \t");
    size_t last = str.find_last_not_of(" \t");
    return std::string(str.substr(first, last - first + 1));
}
}

CrawlerConfig CrawlerConfig::FromFile(const std::string& path) {
    CrawlerConfig config;
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    std::string line;
    size_t line_number = 0;
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            throw std::runtime_error(
                "Invalid config line " + std::to_string(line_number) + 
                ": missing '='");
        }

        auto key = trim(std::string_view(line.data(), eq_pos));
        auto value = trim(std::string_view(line.data() + eq_pos + 1));

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
        }
    }

    if (config.seed_urls.empty()) {
        throw std::runtime_error("No seed URLs configured");
    }

    return config;
}

} // namespace mithril