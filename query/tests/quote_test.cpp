#include "../src/Query.h"

#include "TermDictionary.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>

// Helper function for printing timing information
long print_timing(const std::string& operation, const std::chrono::steady_clock::time_point& start_time) {
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double elapsed_sec = elapsed_ms / 1000.0;
    
    // Print a separator line for visibility
    std::string separator(60, '=');
    
    std::cout << std::endl;
    std::cout << separator << std::endl;
    std::cout << "⏱️  PERFORMANCE: " << operation << " completed in " << std::fixed << std::setprecision(3) << elapsed_sec << " seconds" << std::endl;
    std::cout << separator << std::endl;
    std::cout << std::endl;
    
    return elapsed_ms; // Return the time in milliseconds
}

// Function to print results
void print_results(const std::vector<uint32_t>& results, const std::string& quoted_phrase) {
    // Print results
    std::cout << "Found " << results.size() << " documents containing the phrase '"
            << quoted_phrase << "'\n";
    
    // Show at most 10 results
    const size_t max_to_show = 10;
    const size_t num_to_show = std::min(results.size(), max_to_show);
    
    if (num_to_show > 0) {
        std::cout << "Top " << num_to_show << " document IDs:" << std::endl;
        for (size_t i = 0; i < num_to_show; ++i) {
            std::cout << i + 1 << ". Document ID: " << results[i] << std::endl;
            std::cout << "---------" << std::endl;
        }
        
        if (results.size() > max_to_show) {
            std::cout << "... and " << results.size() - max_to_show << " more documents" << std::endl;
        }
    }
}

// Collect documents using ISR approach
std::vector<uint32_t> collect_from_isr(std::unique_ptr<mithril::IndexStreamReader> isr, size_t max_docs = 100000) {
    std::vector<uint32_t> results;
    results.reserve(max_docs);
    
    while (isr->hasNext() && results.size() < max_docs) {
        results.push_back(isr->currentDocID());
        isr->moveNext();
    }
    
    return results;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <index_path> <quoted_phrase> <mode>\n";
        std::cerr << "Example: " << argv[0] << " ./my_index \"computer science\" direct\n";
        std::cerr << "Modes: direct, isr\n";
        return 1;
    }
    
    // Set the index path
    query::QueryConfig::GetIndexPath() = std::string(argv[1]);

    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    // Determine which mode to run
    std::string mode = argv[3];
    if (mode != "direct" && mode != "isr") {
        std::cerr << "Invalid mode: " << mode << "\n";
        std::cerr << "Valid modes: direct, isr\n";
        return 1;
    }
    
    std::cout << "Using index at: '" << query::QueryConfig::GetIndexPath() << "'" << std::endl;
    std::cout << "Searching for phrase: '" << argv[2] << "'" << std::endl;
    std::cout << "Mode: " << mode << std::endl;
    
    try {
        // Create a quote query from command line argument
        QuoteQuery quoteQuery(Token(TokenType::QUOTE, argv[2]), index_file, term_dict, position_index);
        
        std::vector<uint32_t> results;
        
        // Run query based on selected mode
        if (mode == "direct") {
            std::cout << "\n===== RUNNING DIRECT EVALUATION =====\n";
            
            // Measure query evaluation time
            auto query_start = std::chrono::steady_clock::now();
            
            // Execute the query using direct evaluation
            results = quoteQuery.evaluate();
            
            // Print timing information
            print_timing("Direct QUOTE evaluation", query_start);
        }
        else if (mode == "isr") {
            std::cout << "\n===== RUNNING ISR STREAMING =====\n";
            
            // Measure query evaluation time using ISR
            auto query_start = std::chrono::steady_clock::now();
            
            // Generate an ISR for the QUOTE query
            auto quote_isr = quoteQuery.generate_isr();
            
            // Collect results by iterating through the ISR
            results = collect_from_isr(std::move(quote_isr));
            
            // Print timing information
            print_timing("ISR QUOTE streaming", query_start);
        }
        
        // Print results
        print_results(results, argv[2]);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}