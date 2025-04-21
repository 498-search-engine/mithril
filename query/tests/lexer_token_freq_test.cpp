
#include "Parser.h"
#include "Lexer.h"
#include "Token.h"

#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <unordered_map>


void PrintFrequencies(const std::unordered_map<std::string, int>& freqs) {
    std::cout << "Frequencies:\n";
    for (const auto& [val, count] : freqs) {
        std::cout << "  \"" << val << "\": " << count << "\n";
    }
}

// Assumes Lexer has GetTokenFrequencies() and PeekWithoutConsuming() methods.
int main() {
    std::string input = "TITLE:book \"John Doe\" AND TITLE:book";

    Lexer lexer(input);

    auto freqs = lexer.GetTokenFrequencies();
    PrintFrequencies(freqs);

    return 0;
}
