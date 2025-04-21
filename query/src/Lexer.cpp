#include "Lexer.h"

#include "QueryConfig.h"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

Lexer::Lexer(const std::string& input) : input_(input), position_(0), hasPeeked_(false) {}

// * ------- Public API -------

auto Lexer::NextToken() -> Token {

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
    } else if (c == '\'') {
        return LexSingleQuotedPhrase();
    } else if (c == ':' || c == '(' || c == ')') {
        return LexSymbol();
    } else {
        // Allow any other character to be part of a word
        return LexWordOrKeyword();
    }

    // This line will never be reached as all cases are handled above
    // throw std::runtime_error(std::string("Unexpected character: ") + c);
}

auto Lexer::PeekToken() -> Token {
    if (!hasPeeked_) {
        peekedToken_ = NextToken();
        hasPeeked_ = true;
    }
    return peekedToken_;
}

auto Lexer::EndOfInput() -> bool {
    return PeekToken().type == TokenType::EOFTOKEN;
}

std::unordered_map<std::string, int> Lexer::GetTokenFrequencies() const {
    auto tokens = PeekWithoutConsuming();
    std::unordered_map<std::string, int> token_ct;
    for (auto& token : tokens){
        if (token.type == TokenType::WORD or token.type == TokenType::QUOTE) {
            ++token_ct[token.value];
        }
    }
    return token_ct;
}

// Private helpers

std::vector<Token> Lexer::PeekWithoutConsuming() const {
    Lexer copy = *this;
    std::vector<Token> tokens;

    while (!copy.EndOfInput()) {
        tokens.push_back(copy.NextToken());
    }

    return tokens;
}

void Lexer::SkipWhitespace() {
    while (position_ < input_.length() && std::isspace(input_[position_])) {
        ++position_;
    }
}

auto Lexer::PeekChar() const -> char {
    return input_[position_];
}

auto Lexer::GetChar() -> char {
    return input_[position_++];
}

auto Lexer::MatchChar(char expected) -> bool {
    if (position_ < input_.length() && input_[position_] == expected) {
        ++position_;
        return true;
    }
    return false;
}

auto Lexer::IsAlpha(char c) const -> bool {
    return std::isalpha(static_cast<unsigned char>(c));
}

auto Lexer::IsAlnum(char c) const -> bool {
    return std::isalnum(static_cast<unsigned char>(c));
}

auto Lexer::IsOperatorKeyword(const std::string& word) const -> bool {
    return query::QueryConfig::GetValidOperators().contains(word);
}

auto Lexer::IsFieldKeyword(const std::string& word) const -> bool {
    return query::QueryConfig::GetValidFields().contains(word);
}

// Lex individual token types

auto Lexer::LexWordOrKeyword() -> Token {
    size_t start = position_;
    // Allow any characters except whitespace and special symbols
    while (position_ < input_.length() && 
           !std::isspace(input_[position_]) && 
           input_[position_] != ':' && 
           input_[position_] != '(' && 
           input_[position_] != ')' && 
           input_[position_] != '"' && 
           input_[position_] != '\'') {
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

auto Lexer::LexQuotedPhrase() -> Token {
    GetChar();  // Consume the opening quote

    std::string phrase;
    while (position_ < input_.length()) {
        char const c = GetChar();
        if (c == '"') {
            return Token{TokenType::QUOTE, phrase};
        }
        phrase += c;
    }

    throw std::runtime_error("Unterminated quoted phrase");
}

auto Lexer::LexSingleQuotedPhrase() -> Token {
    GetChar();  // Consume the opening single quote

    std::string phrase;
    while (position_ < input_.length()) {
        char const c = GetChar();
        if (c == '\'') {
            return Token{TokenType::PHRASE, phrase};
        }
        phrase += c;
    }

    throw std::runtime_error("Unterminated single quoted phrase");
}

auto Lexer::LexSymbol() -> Token {
    char const c = GetChar();

    switch (c) {
    case ':':
        return {TokenType::COLON, ":"};
    case '(':
        return {TokenType::LPAREN, "("};
    case ')':
        return {TokenType::RPAREN, ")"};
    default:
        throw std::runtime_error(std::string("Unexpected symbol: ") + c);
    }
}
