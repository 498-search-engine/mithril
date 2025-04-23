#include "QueryCoordinator.h"

#include "NetworkHelper.h"
#include "TextPreprocessor.h"
#include "network.h"
#include "rpc_handler.h"

#include <algorithm>
#include <core/thread.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

using namespace core;
using namespace mithril;

using QueryResults = QueryManager::QueryResult;

mithril::QueryCoordinator::QueryCoordinator(const std::string& conf_path) {
    try {
        std::string file_contents = ReadFile(conf_path.c_str());
        auto lines = GetLines(file_contents);

        if (lines.size() < 2) {
            throw std::runtime_error("Configuration file must have at least 2 lines");
        }

        // Skip first line (header)
        for (size_t i = 1; i < lines.size(); i++) {
            auto line = lines[i];
            if (line.empty())
                continue;

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

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load server configuration: " + std::string(e.what()));
    }
}

void mithril::QueryCoordinator::print_server_configs() const {
    for (const auto& config : server_configs_) {
        spdlog::info("Server IP: {}, Port: {}", config.ip, config.port);
    }
}

QueryResults mithril::QueryCoordinator::send_query_to_workers(const std::string& query) {
    mithril::TokenNormalizer token_normalizer;
    // std::string normalized_query = token_normalizer.normalize(query);
    auto normalized_query = query;

    if (normalized_query.empty()) {
        spdlog::warn("Normalized query is empty");
        return {};
    }

    std::vector<QueryResults> worker_results;
    std::vector<std::future<QueryResults>> futures;

    // Create futures for each worker
    for (const auto& config : server_configs_) {
        // gotta use a lambda as handle_worker_response is not a static method
        futures.push_back(std::async(std::launch::async, [this, config, normalized_query]() {
            return this->handle_worker_response(config, normalized_query);
        }));
    }

    // Collect results
    for (auto& future : futures) {
        try {
            auto results = future.get();
            worker_results.push_back(std::move(results));
        } catch (const std::exception& e) {
            spdlog::error("Worker error: {}", e.what());
        }
    }

    // Aggregate all results
    QueryResults all_results;
    for (const auto& results : worker_results) {
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    // Sort and remove duplicates (unneeded imo as diff shards)
    // std::sort(all_results.begin(), all_results.end());
    // auto last = std::unique(all_results.begin(), all_results.end());
    // all_results.erase(last, all_results.end());

    spdlog::info("Aggregated {} results from {} workers", all_results.size(), worker_results.size());

    return all_results;
}

QueryResults mithril::QueryCoordinator::handle_worker_response(const ServerConfig& server_config,
                                                                        const std::string& query) {

    QueryResults results;

    try {
        int client_fd = create_client_sockfd(server_config.ip.c_str(), server_config.port);
        if (client_fd == -1) {
            throw std::runtime_error("Failed to create client socket");
        }

        // Binary protocol
        // 1. Send query length
        uint32_t query_length = query.length();
        send(client_fd, &query_length, sizeof(uint32_t), 0);

        // 2. Send query string
        send(client_fd, query.c_str(), query_length, 0);

        // // 3. Receive result count
        // uint32_t result_count = 0;
        // ssize_t bytes_read = recv(client_fd, &result_count, sizeof(uint32_t), MSG_WAITALL);

        // if (bytes_read != sizeof(uint32_t)) {
        //     throw std::runtime_error("Failed to read result count");
        // }

        // // 4. Receive document IDs
        // if (result_count > 0) {
        //     results.resize(result_count);
        //     bytes_read = recv(client_fd, results.data(), result_count * sizeof(uint32_t), MSG_WAITALL);

        //     if (bytes_read != result_count * sizeof(uint32_t)) {
        //         throw std::runtime_error("Failed to read full results");
        //     }
        // }

        results = RPCHandler::ReadResults(client_fd);

        close(client_fd);

        spdlog::info("Received {} results from worker at {}:{}", results.size(), server_config.ip, server_config.port);
    } catch (const std::exception& e) {
        spdlog::error("Error communicating with worker at {}:{}: {}", server_config.ip, server_config.port, e.what());
    }

    return results;
}
