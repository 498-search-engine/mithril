#include "Parser.h"

#include "TextPreprocessor.h"
#include "Lexer.h"
#include "PositionIndex.h"
#include "Query.h"
#include "QueryConfig.h"
#include "TermDictionary.h"
#include "Token.h"
#include "core/mem_map_file.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


mithril::Parser::Parser(const std::string& input,
                        const core::MemMapFile& index_file,
                        TermDictionary& term_dict,
                        PositionIndex& position_index)
    : input_(input),
      index_file_(index_file),
      term_dict_(term_dict),
      position_index_(position_index),
      current_position_(0) {
    Lexer lexer(input);
    while (!lexer.EndOfInput()) {
        auto token = lexer.NextToken();

        // token.value = TokenNormalizer::normalize(token.value);

        // if (token.value.empty()) {
        //     continue;
        // }

        tokens_.push_back(token);
    }
}
auto mithril::Parser::get_tokens() const -> const std::vector<Token>& {
    return tokens_;
}
std::unique_ptr<Query> mithril::Parser::parse() {
    if (tokens_.empty()) {
        throw ParseException("Empty token list");
    }

    auto result = parseExpression();

    if (!isAtEnd()) {
        throw ParseException("Unexpected tokens after expression");
    }

    return result;
}
inline int mithril::Parser::getTokenMultiplicity(std::string& token) {
    return token_mult.contains(token) ? token_mult[token] : 0;
}
bool mithril::Parser::isAtEnd() const {
    return current_position_ >= tokens_.size();
}
Token mithril::Parser::peek() const {
    if (isAtEnd()) {
        throw ParseException("Unexpected end of input");
    }
    return tokens_[current_position_];
}
Token mithril::Parser::advance() {
    if (isAtEnd()) {
        throw ParseException("Unexpected end of input");
    }
    return tokens_[current_position_++];
}
bool mithril::Parser::match(TokenType type) {
    if (isAtEnd() || tokens_[current_position_].type != type) {
        return false;
    }
    current_position_++;
    return true;
}
bool mithril::Parser::matchOperator(const std::string& op) {
    if (isAtEnd() || tokens_[current_position_].type != TokenType::OPERATOR || tokens_[current_position_].value != op) {
        return false;
    }
    current_position_++;
    return true;
}
Token mithril::Parser::expect(TokenType type, const std::string& error_message) {
    if (isAtEnd() || tokens_[current_position_].type != type) {
        throw ParseException(error_message);
    }
    return tokens_[current_position_++];
}
std::unique_ptr<Query> mithril::Parser::parseExpression() {
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
            else if (peek().type == TokenType::WORD || peek().type == TokenType::QUOTE ||
                     peek().type == TokenType::FIELD || peek().type == TokenType::LPAREN ||
                     peek().type == TokenType::TITLE || peek().type == TokenType::URL ||
                     peek().type == TokenType::ANCHOR || peek().type == TokenType::DESC) {
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
std::unique_ptr<Query> mithril::Parser::parseQueryComponent() {
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


    if (match(TokenType::WORD) || match(TokenType::TITLE) || match(TokenType::URL) || match(TokenType::ANCHOR) ||
        match(TokenType::DESC)) {
        return std::make_unique<TermQuery>(
            Token(tokens_[current_position_ - 1].type, tokens_[current_position_ - 1].value),
            index_file_,
            term_dict_,
            position_index_);
    }

    // Handle exact matches (quoted terms)
    if (match(TokenType::QUOTE)) {
        // When you implement PhraseQuery, uncomment this:
        // return std::make_unique<PhraseQuery>(tokens_[current_position_ - 1].value);
        // For now, create a term query with the phrase content
        return std::make_unique<QuoteQuery>(tokens_[current_position_ - 1], index_file_, term_dict_, position_index_);
        // return std::make_unique<QuoteQuery>(Token(TokenType::QUOTE, tokens_[current_position_ - 1]),
        //                                    index_file_, term_dict_, position_index_);
    }

    // Handle fuzzy phrase matching
    if (match(TokenType::PHRASE)) {
        // return std::make_unique<PhraseQuery>(tokens_[current_position_ - 1], index_file_, term_dict_,
        // position_index_);
        return std::make_unique<PhraseQuery>(tokens_[current_position_ - 1], index_file_, term_dict_, position_index_);
    }

    // Handle grouped expressions
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    throw ParseException("Expected keyword, field, exact match, or grouped expression");
}
std::unique_ptr<Query> mithril::Parser::parseFieldExpression() {
    Token field = tokens_[current_position_ - 1];
    if (match(TokenType::COLON)) {
        // Now we need either a keyword or exact match
        if (match(TokenType::WORD) || match(TokenType::QUOTE) || match(TokenType::PHRASE)) {
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
void mithril::Parser::makeTokenMap() {
    for (auto& token : tokens_) {
        if (token.type == TokenType::WORD or token.type == TokenType::QUOTE) {
            ++token_mult[token.value];
        }
    }
}
