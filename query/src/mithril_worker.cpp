#include "NetworkHelper.h"
#include "QueryEngine.h"
#include "network.h"

#include <iostream>
#include <string>



constexpr int BUFFER_SIZE = 1024;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --index PATH --port PORT" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --index PATH    Set the index path (required)" << std::endl;
    std::cout << "  --port PORT     Set the server port to listen on (required)" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string indexPath;
    int port = 0;

    // Parse command line arguments
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
        std::cout << "Accepted a child connection\n";
        if (client_fd < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            continue;
        }

        Receive data(client_fd);

        // This code runs the query on one index 
        if (data.data.size() > 0) {
            std::cout << "Received message: " << data.data << std::endl;

            auto tree = queryEngine.ParseQuery(data.data);
            std::cout << "Query tree: " << tree->to_string() << std::endl;
            auto query = queryEngine.EvaluateQuery(data.data);
            queryEngine.DisplayResults(query, 10);

            std::cout << "Query evaluated successfully." << std::endl
                      << "--------------------" << std::endl
                      << std::endl;

            std::string response = "Valid Query and successfully evaluated \n";
            write(client_fd, response.c_str(), response.size());
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
