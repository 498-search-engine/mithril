#include "QueryManager.h"

#include <spdlog/spdlog.h>
#include <string>
#include <vector>

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

    QueryManager qm(index_dirs);
    spdlog::info("Constructed Query Manager with {} workers", index_dirs.size());
    spdlog::info("Now serving queries. Enter below...");
    return 0;
}