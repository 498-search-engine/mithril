#ifndef GRAMMAR_H_
#define GRAMMAR_H_

#include "../../index/src/TextPreprocessor.h"
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

namespace mithril {

class ParseException : public std::runtime_error {
public:
    ParseException(const ParseException&) = default;
    ParseException(ParseException&&) = default;
    ParseException& operator=(const ParseException&) = default;
    ParseException& operator=(ParseException&&) = default;
    explicit ParseException(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    Parser(const Parser&) = default;
    Parser(Parser&&) = default;
    Parser& operator=(const Parser&) = delete;
    Parser& operator=(Parser&&) = delete;
    explicit Parser(const std::string& input,
                    const core::MemMapFile& index_file,
                    TermDictionary& term_dict,
                    PositionIndex& position_index);

    [[nodiscard]] auto get_tokens() const -> const std::vector<Token>&;

    std::unique_ptr<Query> parse();

    inline int getTokenMultiplicity(std::string& token);

private:
    // Navigation and token checking methods
    bool isAtEnd() const;

    Token peek() const;

    Token advance();

    bool match(TokenType type);

    bool matchOperator(const std::string& op);

    Token expect(TokenType type, const std::string& error_message);

    // Grammar parsing methods
    std::unique_ptr<Query> parseExpression();

    std::unique_ptr<Query> parseQueryComponent();

    std::unique_ptr<Query> parseFieldExpression();

    void makeTokenMap();

    std::string input_;  // Store the original input string
    const core::MemMapFile& index_file_;
    TermDictionary& term_dict_;
    PositionIndex& position_index_;
    std::vector<Token> tokens_;
    size_t current_position_;
    std::unordered_map<std::string, int> token_mult;
};

}  // namespace mithril

#endif /* GRAMMAR_H_ */
