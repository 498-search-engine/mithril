#include "lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& input)
    : input(input), position(0), hasPeeked(false) {}

// Public API

Token Lexer::NextToken() {
    if (hasPeeked) {
        hasPeeked = false;
        return peekedToken;
    }

    SkipWhitespace();

    if (position >= input.length()) {
        return Token(TokenType::EOF_TOKEN);
    }

    char c = PeekChar();

    if (std::isalpha(c)) {
        return LexWordOrKeyword();
    } else if (c == '"') {
        return LexQuotedPhrase();
    } else if (c == ':' || c == '(' || c == ')') {
        return LexSymbol();
    }

    throw std::runtime_error(std::string("Unexpected character: ") + c);
}

Token Lexer::PeekToken() {
    if (!hasPeeked) {
        peekedToken = NextToken();
        hasPeeked = true;
    }
    return peekedToken;
}

bool Lexer::EndOfInput() const {
    return position >= input.length();
}

// Private helpers

void Lexer::SkipWhitespace() {
    while (position < input.length() && std::isspace(input[position])) {
        ++position;
    }
}

char Lexer::PeekChar() const {
    return input[position];
}

char Lexer::GetChar() {
    return input[position++];
}

bool Lexer::MatchChar(char expected) {
    if (position < input.length() && input[position] == expected) {
        ++position;
        return true;
    }
    return false;
}

bool Lexer::IsAlpha(char c) const {
    return std::isalpha(static_cast<unsigned char>(c));
}

bool Lexer::IsAlnum(char c) const {
    return std::isalnum(static_cast<unsigned char>(c));
}

bool Lexer::IsOperatorKeyword(const std::string& word) const {
    return word == "AND" || word == "OR" || word == "NOT";
}

bool Lexer::IsFieldKeyword(const std::string& word) const {
    return word == "TITLE" || word == "TEXT";
}

// Lex individual token types

Token Lexer::LexWordOrKeyword() {
    size_t start = position;
    while (position < input.length() && IsAlnum(input[position])) {
        ++position;
    }

    std::string word = input.substr(start, position - start);

    if (IsOperatorKeyword(word)) {
        return Token(TokenType::OPERATOR, word);
    } else if (IsFieldKeyword(word)) {
        return Token(TokenType::FIELD, word);
    } else {
        return Token(TokenType::WORD, word);
    }
}

Token Lexer::LexQuotedPhrase() {
    GetChar();  // Consume the opening quote

    std::string phrase;
    while (position < input.length()) {
        char c = GetChar();
        if (c == '"') {
            return Token(TokenType::PHRASE, phrase);
        }
        phrase += c;
    }

    throw std::runtime_error("Unterminated quoted phrase");
}

Token Lexer::LexSymbol() {
    char c = GetChar();

    switch (c) {
        case ':': return Token(TokenType::COLON, ":");
        case '(': return Token(TokenType::LPAREN, "(");
        case ')': return Token(TokenType::RPAREN, ")");
        default:
            throw std::runtime_error(std::string("Unexpected symbol: ") + c);
    }
}
