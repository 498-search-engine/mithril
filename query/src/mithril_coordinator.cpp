#include "NetworkHelper.h"
#include "QueryCoordinator.h"
#include "network.h"

#include <iostream>
#include <string>

constexpr int BUFFER_SIZE = 1024;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --conf SERVER_CONFIG_PATH" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --conf SERVER_CONFIG_PATH    Set the server config path (required)" << std::endl;
}

int main(int argc, char* argv[]) {

    std::string confPath;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--conf" && i + 1 < argc) {
            confPath = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (confPath.empty()) {
        std::cerr << "Error: --conf is required argument." << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    mithril::QueryCoordinator queryCoordinator(confPath);

    queryCoordinator.print_server_configs();

    std::string query;

    while (true) {
        std::cout << "Enter your search query (or 'Ctrl-C' to quit): ";
        std::getline(std::cin, query);

        if (query.empty()) {
            continue;
        }

        try {
            queryCoordinator.send_query_to_workers(query);
        } catch (const std::exception& e) {
            std::cerr << "Error processing query: " << e.what() << std::endl;
        }
    }

    return 0;
}
