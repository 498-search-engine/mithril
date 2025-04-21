/*
 * Lexer.h
 *
 * Declaration of a stream of tokens that you can read from.
 *
 * You do not have to modify this file, but you may choose to do so.
 */

#ifndef LEXER_H_
#define LEXER_H_

#include "Token.h"

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

class Lexer {
public:
    // Constructor that initializes the lexer with the input string
    explicit Lexer(const std::string& input);

    /**
     * Returns the next token in the stream.
     * If at end of input, returns TokenType::EOF_TOKEN.
     * Consumes the token as well.
     */
    auto NextToken() -> Token;

    /**
     * Returns the next token without consuming it.
     * This allows for peeking at the next token in the stream.
     */
    auto PeekToken() -> Token;

    /**
     * Checks if the input has been fully consumed.
     * Returns true if there are no more tokens to read.
     */
    [[nodiscard]] auto EndOfInput() -> bool;

    /** 
     * Goes through the query and returns a frequency count of all the tokens
    */
    [[nodiscard]] auto GetTokenFrequencies() const -> std::unordered_map<std::string, int>;


private:
    std::string input_;                                   // The input string to be tokenized
    size_t position_ = 0;                                 // Current position in the input string
    Token peekedToken_ = Token(TokenType::EOFTOKEN, "");  // Token that has been peeked
    bool hasPeeked_ = false;                              // Flag to indicate if a token has been peeked

    // Core lexing helpers
    void SkipWhitespace();  // Skips any whitespace characters in the input

    [[nodiscard]] auto PeekChar() const -> char;       // Returns the current character without consuming it
    auto GetChar() -> char;                            // Consumes and returns the current character
    auto MatchChar(char expected) -> bool;             // Matches the current character with the expected character
    [[nodiscard]] auto IsAlpha(char c) const -> bool;  // Checks if a character is an alphabetic character
    [[nodiscard]] auto IsAlnum(char c) const -> bool;  // Checks if a character is alphanumeric
    [[nodiscard]] auto IsOperatorKeyword(const std::string& word) const
        -> bool;  // Checks if a word is an operator keyword
    [[nodiscard]] auto IsFieldKeyword(const std::string& word) const -> bool;  // Checks if a word is a field keyword

    // Token lexing functions
    auto LexWordOrKeyword() -> Token;  // Lexes a word or keyword and returns the corresponding token
    auto LexQuotedPhrase() -> Token;   // Lexes a quoted phrase and returns the corresponding token
    auto LexSingleQuotedPhrase() -> Token;  // Lexes a single quoted phrase and returns the corresponding token
    auto LexSymbol() -> Token;         // Lexes symbols (COLON, LPAREN, RPAREN) and returns the corresponding token
    auto PeekWithoutConsuming() const -> std::vector<Token>; //Returns a vector of all tokens without consuming
};

#endif  // LEXER_H_
