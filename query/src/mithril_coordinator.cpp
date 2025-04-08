#include "NetworkHelper.h"
#include "network.h"
#include "QueryCoordinator.h"
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

    mithril::QueryCoordinator const queryCoordinator(confPath);
    queryCoordinator.print_server_configs();
    return 0;
}
