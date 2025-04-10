#ifndef GRAMMAR_H_
#define GRAMMAR_H_

#include "Lexer.h"
#include "Query.h"
#include "Token.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include "QueryConfig.h"
#include "../../index/src/TermDictionary.h"

namespace mithril {

class ParseException : public std::runtime_error {
public:
    explicit ParseException(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    // Updated constructor to take a string and tokenize it using Lexer
    explicit Parser(const std::string& input, TermDictionary& term_dict) : 
        input_(input),
        term_dict_(term_dict),
        current_position_(0) {

        Lexer lexer(input);
        while (!lexer.EndOfInput()) {
            tokens_.push_back(lexer.NextToken());
        }
    }

    [[nodiscard]] auto get_tokens() const -> const std::vector<Token>& {
        return tokens_;
    }

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
        try {
            // Parse the first query component
            auto leftComponent = parseQueryComponent();
            
            // Continue parsing operators and right-hand expressions as long as there are more tokens
            while (!isAtEnd()) {
                // If we see an operator, process the next component
                if (matchOperator("AND") || matchOperator("OR") || matchOperator("NOT")) {
                    std::string op = tokens_[current_position_ - 1].value;
                    auto rightComponent = parseQueryComponent();
                    
                    if (op == "AND") {
                        leftComponent = std::make_unique<AndQuery>(leftComponent.release(), rightComponent.release());
                    } else if (op == "OR") {
                        leftComponent = std::make_unique<OrQuery>(leftComponent.release(), rightComponent.release());
                    } else if (op == "NOT") {
                        return std::make_unique<NotQuery>(rightComponent.release());
                    }
                } 
                // If there's no operator but we have another component, treat as implicit AND
                else if (peek().type == TokenType::WORD || 
                         peek().type == TokenType::QUOTE || 
                         peek().type == TokenType::FIELD || 
                         peek().type == TokenType::LPAREN) {
                    auto rightComponent = parseQueryComponent();
                    leftComponent = std::make_unique<AndQuery>(leftComponent.release(), rightComponent.release());
                }
                // If we see something else (like a closing parenthesis), break out and return
                else {
                    break;
                }
            }
            
            return leftComponent;
        } catch (const ParseException& e) {
            throw ParseException("Expression error: " + std::string(e.what()));
        }
    }
    
    std::unique_ptr<Query> parseQueryComponent() {
        // Handle NOT operator as a prefix
        if (matchOperator("NOT")) {
            auto operand = parseQueryComponent();
            return std::make_unique<NotQuery>(operand.release());
        }
        
        // Handle field expressions
        if (match(TokenType::FIELD)) {
            return parseFieldExpression();
        }
        
        // Handle keywords (simple terms)
        if (match(TokenType::WORD)) {
            return std::make_unique<TermQuery>(Token(TokenType::WORD, tokens_[current_position_ - 1].value), term_dict_);
        }
        
        // Handle exact matches (quoted terms)
        if (match(TokenType::QUOTE)) {
            // When you implement PhraseQuery, uncomment this:
            // return std::make_unique<PhraseQuery>(tokens_[current_position_ - 1].value);
            // For now, create a term query with the phrase content
            return std::make_unique<TermQuery>(Token(TokenType::QUOTE, tokens_[current_position_ - 1].value), term_dict_);
        }
        
        // Handle grouped expressions
        if (match(TokenType::LPAREN)) {
            auto expr = parseExpression();
            expect(TokenType::RPAREN, "Expected ')' after expression");
            return expr;
        }
        
        throw ParseException("Expected keyword, field, exact match, or grouped expression");
    }
    
    std::unique_ptr<Query> parseFieldExpression() {
        Token field = tokens_[current_position_ - 1];
        if (match(TokenType::COLON)) {
            // Now we need either a keyword or exact match
            if (match(TokenType::WORD) || match(TokenType::QUOTE)) {
                Token term = tokens_[current_position_ - 1];
                // When you implement FieldQuery, uncomment this:
                // return std::make_unique<FieldQuery>(field.value, 
                //     std::make_unique<TermQuery>(term).release());
                throw ParseException("Field queries not yet implemented");
            }
            throw ParseException("Expected keyword or exact match after field specifier");
        }
        throw ParseException("Expected ':' after field name");
    }
    
    std::string input_;  // Store the original input string
    TermDictionary& term_dict_;
    std::vector<Token> tokens_;
    size_t current_position_;
};

} // namespace mithril

#endif /* GRAMMAR_H_ */
