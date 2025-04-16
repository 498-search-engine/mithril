#include "../src/Query.h"
#include "../src/QueryConfig.h"
#include "../src/Token.h"
#include "TermDictionary.h"
#include "PositionIndex.h"

#include <cstdint>
// #include <_types/_uint32_t.h>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <vector>
#include <spdlog/spdlog.h>

namespace {

void print_results(const std::vector<uint32_t>& docIDs, const std::string& term, size_t max_to_show = 10) {
    spdlog::info("Found {} documents containing the term '{}'", docIDs.size(), term);
    
    const size_t num_to_show = std::min(docIDs.size(), max_to_show);
    
    if (num_to_show > 0) {
        spdlog::info("Top {} document IDs:", num_to_show);
        for (size_t i = 0; i < num_to_show; ++i) {
            spdlog::info("{:2}. Document ID: {}", i + 1, docIDs[i]);
            // spdlog::info("{:2}. Term frequency: {}", i + 1, docIDs[i]);
            // spdlog::info("{:2}. Url: {}", i + 1, docIDs[i].url);
            spdlog::info("---------");

        }
        
        if (docIDs.size() > max_to_show) {
            spdlog::info("... and {} more documents", docIDs.size() - max_to_show);
        }
    } else {
        spdlog::info("No documents found containing the term '{}'", term);
    }
}

void print_timing(const std::string& operation, const std::chrono::steady_clock::time_point& start_time) {
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double elapsed_sec = elapsed_ms / 1000.0;
    
    // Print a separator line for visibility
    std::string separator(60, '=');
    
    // Use spdlog's built-in formatting for a more visible timing output
    spdlog::info("");
    spdlog::info(separator);
    spdlog::info("⏱️  PERFORMANCE: {} completed in {:.3f} seconds", operation, elapsed_sec);
    spdlog::info(separator);
    spdlog::info("");
}

} // namespace

int main(int argc, char* argv[]) {
    // Configure logging
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);
    
    // Check command line arguments
    if (argc != 3) {
        spdlog::error("Usage: {} <index_path> <term>", argv[0]);
        spdlog::info("Example: {} ./my_index computer", argv[0]);
        return 1;
    }

    // Set the index path directly
    query::QueryConfig::SetIndexPath(std::string(argv[1]));
    const std::string term = argv[2];

    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    
    spdlog::info("Using index at: '{}'", query::QueryConfig::GetIndexPath());
    spdlog::info("Searching for term: '{}'", term);
    
    try {
        // Create token and query
        Token token(TokenType::WORD, term);
        
        // Measure query evaluation time
        auto query_start = std::chrono::steady_clock::now();
        
        // Create and evaluate query
        TermQuery query(token, term_dict, position_index);
        auto docIDs = query.evaluate();
        
        // Print timing information
        print_timing("Query evaluation", query_start);
        
        // Display results
        print_results(docIDs, term);
        
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}