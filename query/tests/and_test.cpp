#include "../src/Query.h"
#include <iostream>
#include <string>
#include <chrono>

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
void print_results(const std::vector<uint32_t>& results, const char* term1, const char* term2) {
    // Print results
    std::cout << "Found " << results.size() << " documents containing both '"
            << term1 << "' and '" << term2 << "'\n";
    
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

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <index_path> <term1> <term2>\n";
        std::cerr << "Example: " << argv[0] << " ./my_index computer science\n";
        return 1;
    }
    
    // Set the index path
    query::QueryConfig::IndexPath = std::string(argv[1]);
    
    std::cout << "Using index at: '" << query::QueryConfig::IndexPath << "'" << std::endl;
    std::cout << "Searching for terms: '" << argv[2] << "' AND '" << argv[3] << "'" << std::endl;
    
    try {
        // Test regular AndQuery
        std::cout << "\n===== TESTING REGULAR ANDQUERY =====\n";
        
        // Create two term queries from command line arguments
        TermQuery* term1 = new TermQuery(Token(TokenType::WORD, argv[2]));
        TermQuery* term2 = new TermQuery(Token(TokenType::WORD, argv[3]));
        
        // Create an AND query combining both terms
        AndQuery andQuery(term1, term2);
        
        // Measure query evaluation time
        auto query_start = std::chrono::steady_clock::now();
        
        // Execute the query
        auto results = andQuery.Evaluate();
        
        // Print timing information
        long regular_time = print_timing("Regular AndQuery evaluation", query_start);
        
        // Print results
        print_results(results, argv[2], argv[3]);
        
        // Test SIMD AndQuery
        std::cout << "\n===== TESTING SIMD ANDQUERY =====\n";
        
        // Create new term queries
        TermQuery* term1_simd = new TermQuery(Token(TokenType::WORD, argv[2]));
        TermQuery* term2_simd = new TermQuery(Token(TokenType::WORD, argv[3]));
        
        // Create an SIMD AND query combining both terms
        AndQuerySimd andQuerySimd(term1_simd, term2_simd);
        
        // Measure query evaluation time
        query_start = std::chrono::steady_clock::now();
        
        // Execute the query
        auto results_simd = andQuerySimd.Evaluate();
        
        // Print timing information
        long simd_time = print_timing("SIMD AndQuery evaluation", query_start);
        
        // Print results
        print_results(results_simd, argv[2], argv[3]);
        
        // Compare performance
        std::cout << "\n===== PERFORMANCE COMPARISON =====\n";
        double speedup = static_cast<double>(regular_time) / simd_time;
        std::cout << "Regular AndQuery: " << regular_time << " ms\n";
        std::cout << "SIMD AndQuery: " << simd_time << " ms\n";
        std::cout << "Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n";
        
        if (results.size() != results_simd.size()) {
            std::cout << "\nWARNING: Result counts differ between implementations!\n";
        }
        
        // Clean up
        delete term1;
        delete term2;
        delete term1_simd;
        delete term2_simd;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}