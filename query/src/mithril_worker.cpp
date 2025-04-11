#include "NetworkHelper.h"
#include "QueryEngine.h"
#include "network.h"

#include <iostream>
#include <string>
#include <spdlog/spdlog.h>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --index PATH --port PORT" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --index PATH    Set the index path (required)" << std::endl;
    std::cout << "  --port PORT     Set the server port to listen on (required)" << std::endl;
}

void handle_binary_query(int client_fd, QueryEngine& query_engine) {
    try {
        // 1. Receive query length
        uint32_t query_length = 0;
        ssize_t bytes_read = recv(client_fd, &query_length, sizeof(uint32_t), MSG_WAITALL);

        if (bytes_read != sizeof(uint32_t)) {
            spdlog::error("Failed to read query length, received {} bytes", bytes_read);
            uint32_t result_count = 0;
            send(client_fd, &result_count, sizeof(uint32_t), 0);
            return;
        }

        // 2. Receive query string
        std::vector<char> query_buffer(query_length + 1, 0);
        bytes_read = recv(client_fd, query_buffer.data(), query_length, MSG_WAITALL);

        if (bytes_read != query_length) {
            spdlog::error("Failed to read query, expected {} bytes but got {}", query_length, bytes_read);
            uint32_t result_count = 0;
            send(client_fd, &result_count, sizeof(uint32_t), 0);
            return;
        }

        std::string query(query_buffer.data(), query_length);
        spdlog::info("Received binary query: '{}'", query);

        // 3. Execute query
        auto results = query_engine.EvaluateQuery(query);

        // 4. Send result count
        uint32_t result_count = results.size();
        send(client_fd, &result_count, sizeof(uint32_t), 0);

        // 5. Send results
        if (result_count > 0) {
            send(client_fd, results.data(), result_count * sizeof(uint32_t), 0);
        }

        spdlog::info("Sent {} results back to client", result_count);
    } catch (const std::exception& e) {
        spdlog::error("Error handling binary query: {}", e.what());
        uint32_t result_count = 0;
        send(client_fd, &result_count, sizeof(uint32_t), 0);
    }
}

int main(int argc, char* argv[]) {
    std::string indexPath;
    int port = 0;

    // Parse cli
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--index" && i + 1 < argc) {
            indexPath = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (indexPath.empty() || port == 0) {
        std::cerr << "Error: --index and --port are required arguments." << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    int server_fd = create_server_sockfd(port, 10);

    if (server_fd == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        exit(1);
    }

    std::cout << "Server running on localhost:" << port << std::endl;
    std::cout << "Using index path: " << indexPath << std::endl;

    QueryEngine queryEngine(indexPath);

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        std::cout << "Accepted a client connection\n";
        if (client_fd < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            continue;
        }

        handle_binary_query(client_fd, queryEngine);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
