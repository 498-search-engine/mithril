#include "Clock.h"
#include "NetworkHelper.h"
#include "QueryCoordinator.h"
#include "network.h"
#include "spdlog/common.h"

#include <exception>
#include <iostream>
#include <string>
#include <spdlog/spdlog.h>

namespace {

void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --conf SERVER_CONFIG_PATH" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --conf SERVER_CONFIG_PATH    Set the server config path (required)" << std::endl;
    std::cout << "  --query <query string>       Search for the query and exit" << std::endl;
}

void ExecuteQuery(const std::string& query, mithril::QueryCoordinator& coordinator) {
    try {
        auto start = MonotonicTimeUs();
        auto [results, num] = coordinator.send_query_to_workers(query);
        auto end = MonotonicTimeUs();
        auto delta = end - start;
        for (auto& result : results) {
            const auto& url = std::get<2>(result);
            const auto& titleWords = std::get<3>(result);
            std::string title;
            for (const auto& word : titleWords) {
                title.append(word);
                title.push_back(' ');
            }
            if (!titleWords.empty()) {
                title.pop_back();
            }

            spdlog::info("[result] {} \"{}\"", url, title);
        }
        spdlog::debug("{} results returned in {} ms", results.size(), static_cast<double>(delta) / 1000.0);
    } catch (const std::exception& e) {
        std::cerr << "Error processing query: " << e.what() << std::endl;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    std::string confPath;
    std::string singleQuery;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--conf" && i + 1 < argc) {
            confPath = argv[++i];
        } else if (arg == "--query" && i + 1 < argc) {
            singleQuery = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (confPath.empty()) {
        std::cerr << "Error: --conf is required argument." << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }

    mithril::QueryCoordinator queryCoordinator(confPath);
    queryCoordinator.print_server_configs();

    if (!singleQuery.empty()) {
        ExecuteQuery(singleQuery, queryCoordinator);
        return 0;
    }

    std::string query;
    while (true) {
        std::cout << "Enter your search query (or 'Ctrl-C' to quit): ";
        std::getline(std::cin, query);
        if (query.empty()) {
            continue;
        }
        ExecuteQuery(query, queryCoordinator);
    }

    return 0;
}
