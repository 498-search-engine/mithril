#ifndef GRAMMAR_H_
#define GRAMMAR_H_

#include "Lexer.h"
#include "Query.h"
#include "Token.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>

namespace mithril {

class ParseException : public std::runtime_error {
public:
    explicit ParseException(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    // Updated constructor to take a string and tokenize it using Lexer
    explicit Parser(const std::string& input) : 
        input_(input), 
        current_position_(0) {
        // Use Lexer to tokenize the input
        Lexer lexer(input);
        while (!lexer.EndOfInput()) {
            tokens_.push_back(lexer.NextToken());
        }
        // Remove EOF token if present
        if (!tokens_.empty() && tokens_.back().type == TokenType::EOFTOKEN) {
            tokens_.pop_back();
        }
    }

    // Keep the existing constructor for flexibility
    explicit Parser(const std::vector<Token>& tokens) : 
        tokens_(tokens), 
        current_position_(0) {}

    std::unique_ptr<Query> parse() {
        if (tokens_.empty()) {
            throw ParseException("Empty token list");
        }

        auto result = parseExpression();
        
        if (!isAtEnd()) {
            throw ParseException("Unexpected tokens after expression");
        }

        return result;
    }
    
private:
    // Navigation and token checking methods
    bool isAtEnd() const {
        return current_position_ >= tokens_.size();
    }
    
    Token peek() const {
        if (isAtEnd()) {
            throw ParseException("Unexpected end of input");
        }
        return tokens_[current_position_];
    }
    
    Token advance() {
        if (isAtEnd()) {
            throw ParseException("Unexpected end of input");
        }
        return tokens_[current_position_++];
    }
    
    bool match(TokenType type) {
        if (isAtEnd() || tokens_[current_position_].type != type) {
            return false;
        }
        current_position_++;
        return true;
    }
    
    bool matchOperator(const std::string& op) {
        if (isAtEnd() || tokens_[current_position_].type != TokenType::OPERATOR || 
            tokens_[current_position_].value != op) {
            return false;
        }
        current_position_++;
        return true;
    }
    
    Token expect(TokenType type, const std::string& error_message) {
        if (isAtEnd() || tokens_[current_position_].type != type) {
            throw ParseException(error_message);
        }
        return tokens_[current_position_++];
    }
    
    // Grammar parsing methods
    std::unique_ptr<Query> parseExpression() {
        return parseOr();
    }
    
    std::unique_ptr<Query> parseOr() {
        auto left = parseAnd();
        
        while (matchOperator("OR")) {
            auto right = parseAnd();
            left = std::make_unique<OrQuery>(left.release(), right.release());
        }
        
        return left;
    }
    
    std::unique_ptr<Query> parseAnd() {
        auto left = parseTerm();
        
        while (matchOperator("AND") || matchOperator(" ")) {  // Space can represent implicit AND
            auto right = parseTerm();
            left = std::make_unique<AndQuery>(left.release(), right.release());
        }
        
        return left;
    }
    
    std::unique_ptr<Query> parseTerm() {
        // Handle NOT operator (when implemented)
        if (matchOperator("NOT")) {
            auto operand = parseTerm();
            // When you implement NotQuery, uncomment this:
            // return std::make_unique<NotQuery>(operand.release());
            throw ParseException("NOT operator not yet implemented");
        }
        
        // Handle field queries (when implemented)
        if (match(TokenType::FIELD)) {
            Token field = tokens_[current_position_ - 1];
            if (match(TokenType::COLON)) {
                auto term = parseTerm();
                // When you implement FieldQuery, uncomment this:
                // return std::make_unique<FieldQuery>(field.value, term.release());
                throw ParseException("Field queries not yet implemented");
            }
            throw ParseException("Expected ':' after field name");
        }
        
        // Handle parenthesized expressions
        if (match(TokenType::LPAREN)) {
            auto expr = parseExpression();
            expect(TokenType::RPAREN, "Expected ')' after expression");
            return expr;
        }
        
        // Handle simple terms and phrases
        if (match(TokenType::WORD)) {
            return std::make_unique<TermQuery>(Token(TokenType::WORD, tokens_[current_position_ - 1].value));
        }
        
        if (match(TokenType::PHRASE)) {
            // When you implement PhraseQuery, uncomment this:
            // return std::make_unique<PhraseQuery>(tokens_[current_position_ - 1].value);
            // For now, create a term query with the phrase content
            return std::make_unique<TermQuery>(Token(TokenType::PHRASE, tokens_[current_position_ - 1].value));
        }
        
        throw ParseException("Expected term, field, phrase, or '('");
    }
    
    std::string input_;  // Store the original input string
    std::vector<Token> tokens_;
    size_t current_position_;
};

} // namespace mithril

#endif /* GRAMMAR_H_ */
