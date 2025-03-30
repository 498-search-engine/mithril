#include "../src/Lexer.h"
#include "../src/Parser.h"
#include "../src/Query.h"
#include "../src/QueryConfig.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>

using namespace mithril;

// Helper function to visualize query structure
void printQueryStructure(const std::unique_ptr<Query>& query, int depth = 0) {
    // Print indentation based on depth
    std::string indent(depth * 2, ' ');
    
    if (auto* termQuery = dynamic_cast<TermQuery*>(query.get())) {
        // Display term query details
        std::cout << indent << "TermQuery: \"" << termQuery->get_token().value << "\"" << std::endl;
    }
    else if (auto* andQuery = dynamic_cast<AndQuery*>(query.get())) {
        // Display AND operation
        std::cout << indent << "AndQuery:" << std::endl;
        // We can't directly access left_ and right_ since they're private
        // This is just a placeholder for the structure visualization
        std::cout << indent << "  (cannot display children due to encapsulation)" << std::endl;
    }
    else if (auto* orQuery = dynamic_cast<OrQuery*>(query.get())) {
        // Display OR operation
        std::cout << indent << "OrQuery:" << std::endl;
        // We can't directly access left_ and right_ since they're private
        // This is just a placeholder for the structure visualization
        std::cout << indent << "  (cannot display children due to encapsulation)" << std::endl;
    }
    else if (query) {
        // Default case for other query types
        std::cout << indent << "Unknown Query Type" << std::endl;
    }
    else {
        std::cout << indent << "NULL Query" << std::endl;
    }
}

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
    
    // Set the index path in the QueryConfig
    query::QueryConfig::IndexPath = indexPath;
    std::cout << "Using index path: " << query::QueryConfig::IndexPath << std::endl;
    
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
        std::cout << "Enter a query to parse: ";
        std::getline(std::cin, input);
    }
    
    std::cout << "Parsing query: \"" << input << "\"" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    
    try {
        // Create lexer with the input
        Lexer lexer(input);
        
        // Collect all tokens
        std::vector<Token> tokens;
        while (!lexer.EndOfInput()) {
            Token token = lexer.NextToken();
            tokens.push_back(token);
            
            // Stop if we reach EOF token
            if (token.type == TokenType::EOFTOKEN) {
                break;
            }
        }
        
        // Display tokens for reference
        std::cout << "Tokens:" << std::endl;
        for (size_t i = 0; i < tokens.size(); ++i) {
            const auto& token = tokens[i];
            std::cout << "  " << i+1 << ": ";
            std::cout << token.toString() << " \"" << token.value << "\"" << std::endl;
        }

        return 0; 
        
        // Parse tokens
        Parser parser(tokens);
        std::unique_ptr<Query> queryTree = parser.parse();
        
        // Display query structure
        std::cout << "\nParsed Query Structure:" << std::endl;
        std::cout << "-----------------------------------" << std::endl;
        printQueryStructure(queryTree);
        
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
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}