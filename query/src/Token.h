#ifndef TOKEN_H_
#define TOKEN_H_

#include <string>
#include <vector> 

enum class TokenType {
    WORD,   // simple_term: alphanumeric word
    QUOTE,  // quoted_term: quoted phrase
    PHRASE, // fuzzy phrase matching, looser than QUOTE
    FIELD,     // TITLE or TEXT
    COLON,     // ':'
    OPERATOR,  // AND, OR, NOT, or implicit SPACE
    LPAREN,    // '('
    RPAREN,    // ')'
    EOFTOKEN   // end of input
};

struct Token {
    TokenType type;
    std::string value;

    Token(TokenType t, std::string v = "") : type(t), value(std::move(v)) {}

    // Returns a string representation of the token
    std::string toString() const {
        std::string typeStr;
        switch (type) {
        case TokenType::WORD:
            typeStr = "WORD";
            break;
        case TokenType::QUOTE:
            typeStr = "QUOTE";
            break;
        case TokenType::PHRASE:
            typeStr = "PHRASE";
            break;
        case TokenType::FIELD:
            typeStr = "FIELD";
            break;
        case TokenType::COLON:
            typeStr = "COLON";
            break;
        case TokenType::OPERATOR:
            typeStr = "OPERATOR";
            break;
        case TokenType::LPAREN:
            typeStr = "LPAREN";
            break;
        case TokenType::RPAREN:
            typeStr = "RPAREN";
            break;
        case TokenType::EOFTOKEN:
            typeStr = "EOF";
            break;
        default:
            typeStr = "UNKNOWN";
        }

        return "[" + typeStr + ": " + value + "]";
    }
};


inline static std::vector<std::string> ExtractQuoteTerms(const Token& quote_token) {
    if (quote_token.type != TokenType::QUOTE && quote_token.type != TokenType::PHRASE) {
        throw std::invalid_argument("Token is not a quote or phrase but you are calling extract_quote_terms");
    }

    std::vector<std::string> terms;
    std::string currentTerm;
    
    for (char c : quote_token.value) {
        if (c == ' ') {
            if (!currentTerm.empty()) {
                terms.push_back(currentTerm);
                currentTerm.clear();
            }
        } else {
            currentTerm += c;
        }
    }
    if (!currentTerm.empty()) {
        terms.push_back(currentTerm);
    }
    return terms;
}

#endif  // TOKEN_H_
