#include "../src/Lexer.h"
#include <iostream>
#include <string>
#include <vector>
#include "../../index/src/TextPreprocessor.h"


auto main(int argc, char* argv[]) -> int {
    bool interactive = true;
    std::string input;
    
    // If command line arguments are provided, use them as input and run once
    if (argc > 1) {
        interactive = false;
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        
        // Join the arguments with spaces
        for (size_t i = 0; i < args.size(); ++i) {
            input += args[i];
            if (i < args.size() - 1) {
                input += " ";
            }
        }
    }

    while (true) {
        // Get input if in interactive mode or first run with arguments
        if (interactive) {
            std::cout << "Enter a phrase to tokenize (or 'exit'/'quit' to end): ";
            std::getline(std::cin, input);
            
            // Check for exit command
            if (input == "exit" || input == "quit") {
                break;
            }
        }
        
        std::cout << "Tokenizing: \"" << input << "\"" << std::endl;
        std::cout << "-----------------------------------" << std::endl;
        
        // Create lexer with the input
        Lexer lexer(input);
        
        // Process all tokens
        int tokenCount = 0;
        while (!lexer.EndOfInput()) {
            const Token token = lexer.NextToken();
            
            // Print token information
            std::cout << "Token " << ++tokenCount << ":" << std::endl;
            std::cout << "  Type: " << token.toString() << std::endl;
            std::cout << "  Value: \"" << token.value << "\""  << " | [normalized value]: " << mithril::TokenNormalizer::normalize(token.value) << std::endl;
            
            // Stop if we reach EOF token
            if (token.type == TokenType::EOFTOKEN) {
                break;
            }
        }
        
        std::cout << "-----------------------------------" << std::endl;
        std::cout << "Total tokens: " << tokenCount << std::endl;
        
        // Exit loop if not in interactive mode (command line arguments)
        if (!interactive) {
            break;
        }
    }
    
    return 0;
}