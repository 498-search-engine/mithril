#include "../src/Lexer.h"
#include "../src/Parser.h"
#include "../src/Query.h"
#include "../src/QueryConfig.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"


#include <iostream>
#include <string>
#include <vector>
#include <memory>

using namespace mithril;

// Helper function to print usage instructions
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] [query]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --index PATH    Set the index path (required)" << std::endl;
    std::cout << "  -h, --help          Display this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "If no query is provided, you will be prompted to enter one." << std::endl;
}

auto main(int argc, char* argv[]) -> int {
    std::string input;
    std::string indexPath;
    std::vector<std::string> queryArgs;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-i" || arg == "--index") {
            if (i + 1 < argc) {
                indexPath = argv[++i];
            } else {
                std::cerr << "Error: Index path argument is missing." << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        }
        else {
            // Collect all other arguments as query terms
            queryArgs.push_back(arg);
        }
    }
    
    // Validate that index path is provided
    if (indexPath.empty()) {
        std::cerr << "Error: Index path is required. Use -i or --index to specify it." << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    DocumentMapReader doc_reader(indexPath);

    // Set the index path in the QueryConfig
    query::QueryConfig::SetIndexPath(indexPath);
    std::cout << "Using index path: " << query::QueryConfig::GetIndexPath() << std::endl;

    TermDictionary term_dict(indexPath);
    // PositionIndex position_index(indexPath);
    core::MemMapFile index_file(indexPath + "/final_index.data");
    
    // If query terms were provided, join them as input
    if (!queryArgs.empty()) {
        for (size_t i = 0; i < queryArgs.size(); ++i) {
            input += queryArgs[i];
            if (i < queryArgs.size() - 1) {
                input += " ";
            }
        }
    } else {
        // Otherwise, prompt for input
        std::cout << "Enter a query to parse (Ctrl+C to exit): ";
        std::getline(std::cin, input);
    }
    
    // Loop to handle multiple queries
    while (true) {
        std::cout << "\nParsing query: \"" << input << "\"" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        try {
            // Create parser with the input
            Parser parser(input, index_file, term_dict, position_index);
      
            // Display tokens for reference
            std::cout << "Tokens:" << std::endl;

            for (size_t i = 0; i < parser.get_tokens().size(); ++i) {
                const auto& token = parser.get_tokens()[i];
                std::cout << "  " << i+1 << ": ";
                std::cout << token.toString() << "\"" << std::endl;
            }

            // Parse tokens
            std::unique_ptr<Query> queryTree = parser.parse();
            
            // Display query structure
            std::cout << "\nParsed Query Structure:" << std::endl;
            std::cout << "-----------------------------------" << std::endl;
            std::cout << queryTree->to_string() << std::endl;
            
            // Example of evaluation (optional)
            std::cout << "\nEvaluating Query..." << std::endl;
            std::cout << "-----------------------------------" << std::endl;
            try {
                std::vector<uint32_t> results = queryTree->evaluate();
                std::cout << "Query returned " << results.size() << " results." << std::endl;
                
                // Display first few results if any
                const size_t maxDisplay = 10;
                if (!results.empty()) {
                    std::cout << "First " << std::min(maxDisplay, results.size()) << " document IDs:" << std::endl;
                    for (size_t i = 0; i < std::min(maxDisplay, results.size()); ++i) {
                        std::cout << "  " << results[i];
                        if (i < std::min(maxDisplay, results.size()) - 1) {
                            std::cout << ", ";
                        }
                    }
                    std::cout << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "Evaluation error: " << e.what() << std::endl;
            }
        }
        catch (const ParseException& e) {
            std::cerr << "Parse error: " << e.what() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        
        // Prompt for the next query
        std::cout << "\n-----------------------------------" << std::endl;
        std::cout << "Enter a query to parse (Ctrl+C to exit): ";
        std::getline(std::cin, input);
    }
    
    return 0;
}