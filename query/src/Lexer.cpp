#include "Lexer.h"
#include "QueryConfig.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& input)
    : input_(input), position_(0), hasPeeked_(false) {}

// * ------- Public API -------

Token Lexer::NextToken() {

    if (hasPeeked_) {
        hasPeeked_ = false;
        return peekedToken_;
    }

    SkipWhitespace();

    if (position_ >= input_.length()) {
        return Token(TokenType::EOFTOKEN);
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
    if (!hasPeeked_) {
        peekedToken_ = NextToken();
        hasPeeked_ = true;
    }
    return peekedToken_;
}

bool Lexer::EndOfInput() {
    return PeekToken().type == TokenType::EOFTOKEN;
}

// Private helpers

void Lexer::SkipWhitespace() {
    while (position_ < input_.length() && std::isspace(input_[position_])) {
        ++position_;
    }
}

char Lexer::PeekChar() const {
    return input_[position_];
}

char Lexer::GetChar() {
    return input_[position_++];
}

bool Lexer::MatchChar(char expected) {
    if (position_ < input_.length() && input_[position_] == expected) {
        ++position_;
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
    return query::QueryConfig::GetValidOperators().contains(word);
}

bool Lexer::IsFieldKeyword(const std::string& word) const {
    return query::QueryConfig::GetValidFields().contains(word);
}

// Lex individual token types

Token Lexer::LexWordOrKeyword() {
    size_t start = position_;
    while (position_ < input_.length() && IsAlnum(input_[position_])) {
        ++position_;
    }

    std::string word = input_.substr(start, position_ - start);

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
    while (position_ < input_.length()) {
        char const c = GetChar();
        if (c == '"') {
            return Token{TokenType::PHRASE, phrase};
        }
        phrase += c;
    }

    throw std::runtime_error("Unterminated quoted phrase");
}

Token Lexer::LexSymbol() {
    char const c = GetChar();

    switch (c) {
        case ':': return {TokenType::COLON, ":"};
        case '(': return {TokenType::LPAREN, "("};
        case ')': return {TokenType::RPAREN, ")"};
        default:
            throw std::runtime_error(std::string("Unexpected symbol: ") + c);
    }
}
