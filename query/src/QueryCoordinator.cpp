#include "QueryCoordinator.h"
#include<core/thread.h>
#include "NetworkHelper.h"
#include "network.h"

using namespace core;



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
            
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load server configuration: " + std::string(e.what()));
        }
}
void mithril::QueryCoordinator::print_server_configs() const {
        for (const auto& config : server_configs_) {
            std::cout << "Server IP: " << config.ip << ", Port: " << config.port << std::endl;
        }
}
void mithril::QueryCoordinator::send_query_to_workers(const std::string& query) {
    std::vector<std::vector<uint32_t>> worker_results(server_configs_.size());
    std::vector<core::Thread> threads;
    threads.reserve(server_configs_.size());
    
    // Create all threads
    for (size_t i = 0; i < server_configs_.size(); ++i) {
        threads.emplace_back(handle_worker_response, server_configs_[i], std::ref(worker_results[i]), query);
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.Join();
    }

    // Print all results for each array
    for (size_t i = 0; i < worker_results.size(); ++i) {
        std::cout << "Worker " << i << " results: ";
        for (const auto& result : worker_results[i]) {
            std::cout << result << " ";
        }
        std::cout << std::endl;
    }
    
    // Now worker_results contains all the results from each worker
    // TODO: Process or combine results as needed
}

void mithril::QueryCoordinator::handle_worker_response(const ServerConfig& server_config,
                                                       std::vector<uint32_t>& results,
                                                       const std::string& query) {

    int client_fd = create_client_sockfd(server_config.ip.c_str(), server_config.port);
    if (client_fd == -1) {
        throw std::runtime_error("Failed to create client socket");
    }

    send_message(client_fd, query);
    Receive data(client_fd);
    std::string response = data.data;
    std::cout << "Received response from worker: " << response << std::endl;
}
