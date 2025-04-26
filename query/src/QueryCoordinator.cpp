#include "QueryCoordinator.h"

#include "NetworkHelper.h"
#include "TextPreprocessor.h"
#include "network.h"
#include "rpc_handler.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <core/thread.h>
#include <spdlog/spdlog.h>

#define SOFT_QUERY_TIMEOUT 500

using namespace core;
using namespace mithril;

using QueryResults = QueryManager::QueryResult;

namespace {
template<typename T>
void wait_for_any(std::vector<std::future<T>>& futures) {
    if (futures.empty()) {
        return;
    }

    std::vector<std::shared_future<T>> shared_futures;
    for (auto& f : futures) {
        shared_futures.emplace_back(f.share());
    }

    std::mutex mtx;
    std::condition_variable cv;
    int readyCount = 0;

    for (auto& sf : shared_futures) {
        std::thread([sf, &mtx, &cv, &readyCount]() mutable {
            try {
                sf.wait();
                std::lock_guard<std::mutex> lock(mtx);
                ++readyCount;
                cv.notify_one();
            } catch (std::exception& e) {
                return;
            }
        }).detach();
    }

    std::unique_lock<std::mutex> lock(mtx);

    // Wait until soft query timeout for all threads
    cv.wait_for(lock, std::chrono::milliseconds(SOFT_QUERY_TIMEOUT), [&readyCount, &futures]() {
        return readyCount == futures.size();
    });

    // Then ensure at least one future is done.
    cv.wait(lock, [&readyCount]() { return readyCount > 0; });
}
}  // namespace

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

std::pair<QueryResults, size_t> mithril::QueryCoordinator::send_query_to_workers(const std::string& query) {
    mithril::TokenNormalizer token_normalizer;
    // std::string normalized_query = token_normalizer.normalize(query);
    auto normalized_query = query;
    auto total_results = 0;
    if (normalized_query.empty()) {
        spdlog::warn("Normalized query is empty");
        return {};
    }

    std::vector<QueryResults> worker_results;
    std::vector<std::future<std::pair<QueryResults, size_t>>> futures;

    // Create futures for each worker
    for (const auto& config : server_configs_) {
        // gotta use a lambda as handle_worker_response is not a static method
        futures.push_back(std::async(std::launch::async, [this, config, normalized_query]() {
            return this->handle_worker_response(config, normalized_query);
        }));
    }

    wait_for_any(futures);

    // Collect results
    for (auto& future : futures) {
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                auto results = future.get();
                worker_results.push_back(std::move(results.first));
                total_results += results.second;
            } catch (const std::exception& e) {
                spdlog::error("Worker error: {}", e.what());
            }
        }
    }

    QueryResults all_results = QueryManager::TopKFromSortedLists(worker_results);

    spdlog::info("Aggregated {} results from {} workers which gave {} total results",
                 all_results.size(),
                 worker_results.size(),
                 total_results);

    return {all_results, total_results};
}

std::pair<QueryResults, size_t> mithril::QueryCoordinator::handle_worker_response(const ServerConfig& server_config,
                                                                                  const std::string& query) {

    QueryResults results;
    size_t total_results = 0;
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


        results = RPCHandler::ReadResults(client_fd, total_results);

        close(client_fd);

        spdlog::info("Received {} results from worker at {}:{}", results.size(), server_config.ip, server_config.port);
    } catch (const std::exception& e) {
        spdlog::error("Error communicating with worker at {}:{}: {}", server_config.ip, server_config.port, e.what());
    }

    return {results, total_results};
}
