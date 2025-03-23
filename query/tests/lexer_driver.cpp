#include "../src/Lexer.h"
#include <iostream>
#include <string>
#include <vector>

// Helper function to convert TokenType to string for display
auto TokenTypeToString(TokenType type) -> std::string {
    switch (type) {
        case TokenType::WORD:     return "WORD";
        case TokenType::PHRASE:   return "PHRASE";
        case TokenType::FIELD:    return "FIELD";
        case TokenType::COLON:    return "COLON";
        case TokenType::OPERATOR: return "OPERATOR";
        case TokenType::LPAREN:   return "LPAREN";
        case TokenType::RPAREN:   return "RPAREN";
        case TokenType::EOFTOKEN: return "EOF";
        default:                  return "UNKNOWN";
    }
}

auto main(int argc, char* argv[]) -> int {
    std::string input;
    
    // If command line arguments are provided, join them as input
    if (argc > 1) {
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
    } else {
        // Otherwise, prompt for input
        std::cout << "Enter a phrase to tokenize: ";
        std::getline(std::cin, input);
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
        std::cout << "  Type: " << TokenTypeToString(token.type) << std::endl;
        std::cout << "  Value: \"" << token.value << "\"" << std::endl;
        
        // Stop if we reach EOF token
        if (token.type == TokenType::EOFTOKEN) {
            break;
        }
    }
    
    std::cout << "-----------------------------------" << std::endl;
    std::cout << "Total tokens: " << tokenCount << std::endl;
    
    return 0;
}