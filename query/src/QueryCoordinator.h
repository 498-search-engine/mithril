#ifndef QUERY_COORDINATOR_H_
#define QUERY_COORDINATOR_H_

#include "Parser.h"
#include "Query.h"
#include "QueryConfig.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "Util.h"

namespace mithril {

// class QueryResult {
// public:
//     QueryResult(uint32_t docId, float score) : docId_(docId), score_(score) {}
    
//     uint32_t getDocId() const { return docId_; }
//     float getScore() const { return score_; }
    
// private:
//     uint32_t docId_;
//     float score_;
//     // TODO: Add other result attributes (e.g., title, snippet, etc.)
// };

// struct WorkerInformation {
//     std::string ip;
//     int port;
// };

// Forward declaration of QueryWorker
// class QueryWorker;

// The main coordinator that distributes tasks to workers
class QueryCoordinator {
public:
    QueryCoordinator(const std::string& conf_path) {
        try {
            std::string file_contents = ReadFile(conf_path.c_str());
            auto lines = GetLines(file_contents);
            
            if (lines.size() < 2) {
                throw std::runtime_error("Configuration file must have at least 2 lines");
            }
            
            // Skip first line (header)
            for (size_t i = 1; i < lines.size(); i++) {
                auto line = lines[i];
                if (line.empty()) continue;
                
                // Split line into IP and port
                auto parts = GetWords(line);
                if (parts.size() != 2) {
                    throw std::runtime_error("Invalid server config line: " + std::string(line));
                }
                
                std::string ip(parts[0]);
                uint16_t port = std::stoul(std::string(parts[1]));
                
                server_configs_.push_back({ip, port});
            }
            
            if (server_configs_.empty()) {
                throw std::runtime_error("No valid server configurations found");
            }
            
            // is_running_ = true;
            
            
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load server configuration: " + std::string(e.what()));
        }
    }

    void print_server_configs() const {
        for (const auto& config : server_configs_) {
            std::cout << "Server IP: " << config.ip << ", Port: " << config.port << std::endl;
        }
    }

private:
    // std::string index_dir_;
    // std::queue<std::function<void()>> tasks_;
    // std::mutex queue_mutex_;
    // std::condition_variable condition_;
    
    struct ServerConfig {
        std::string ip;
        uint16_t port;
    };

    std::vector<ServerConfig> server_configs_;
    
    // TODO: Add caching for frequently executed queries
    // TODO: Add query suggestion/autocomplete functionality
    // TODO: Add query expansion/synonym handling
};


} // namespace mithril

#endif // QUERY_COORDINATOR_H_
