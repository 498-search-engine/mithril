#include "QueryManager.h"

#include <iostream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>

using Clock = std::chrono::high_resolution_clock;
using MsBetween = std::chrono::duration<double, std::milli>;

int main(int argc, char** argv) {
    // Configure logging
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);

    // Check command line arguments
    if (argc == 1) {
        spdlog::error("Usage: {} <index_path> |", argv[0]);
        spdlog::info("Example: {} idx1 idx2 idx3", argv[0]);
        return 1;
    }

    spdlog::info("Loading indices");
    std::vector<std::string> index_dirs;
    for (int i = 1; i < argc; ++i) {
        index_dirs.push_back(argv[i]);
    }

    spdlog::info("Making Query Manager");
    QueryManager qm(index_dirs);
    spdlog::info("Constructed Query Manager with {} workers", index_dirs.size());
    spdlog::info("Now serving queries. Enter below...");

    std::string query;
    std::cout << ">> ";
    while (std::getline(std::cin, query)) {
        spdlog::info("Serving query {}...", query);

        auto t0 = Clock::now();
        auto result = qm.AnswerQuery(query);
        auto t1 = Clock::now();

        // Calc runtime
        std::chrono::duration<double, std::milli> query_time = t1 - t0;
        const double query_ms = std::ceil(query_time.count() * 100.0) / 100.0;
        spdlog::info("Found {} matches in {}ms", result.size(), query_ms);
    
        if (result.size() > 0) {
            std::cout << "Best: doc " << std::get<0>(result[0]) << " with score " << std::get<2>(result[0]) << "\n\n";
        }

        std::cout << ">> ";
    }

    return 0;
}
