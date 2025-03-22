/*
 * Lexer.h
 *
 * Declaration of a stream of tokens that you can read from.
 *
 * You do not have to modify this file, but you may choose to do so.
 */

#ifndef LEXER_H_
#define LEXER_H_

#include <string>
#include <utility>


#include "Token.h"

class Lexer {
public:
    explicit Lexer(const std::string& input);

    /**
     * Returns the next token in the stream.
     * If at end of input, returns TokenType::EOF_TOKEN.
     * Consumes it as well 
     */
    auto NextToken() -> Token;

    /**
     * Returns the next token without consuming it.
     */
    auto PeekToken() -> Token;

    /**
     * Checks if the input has been fully consumed.
     */
    [[nodiscard]] auto EndOfInput() const -> bool;

private:
    std::string input_;
    size_t position_ = 0;
    Token peekedToken_ = Token(TokenType::EOFTOKEN, "");
    bool hasPeeked_ = false;

    // Core lexing helpers
    void SkipWhitespace();
    [[nodiscard]] auto PeekChar() const -> char;
    auto GetChar() -> char;
    auto MatchChar(char expected) -> bool;
    [[nodiscard]] auto IsAlpha(char c) const -> bool;
    [[nodiscard]] auto IsAlnum(char c) const -> bool;
    [[nodiscard]] auto IsOperatorKeyword(const std::string& word) const -> bool;
    [[nodiscard]] auto IsFieldKeyword(const std::string& word) const -> bool;

    // Token lexing functions
    auto LexWordOrKeyword() -> Token;      // Handles WORD, OPERATOR, FIELD
    auto LexQuotedPhrase() -> Token;       // Handles PHRASE
    auto LexSymbol() -> Token;             // Handles COLON, LPAREN, RPAREN
};

#endif // LEXER_H_

