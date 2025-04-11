#ifndef TOKEN_H_
#define TOKEN_H_

#include <string>

enum class TokenType {
    WORD,   // simple_term: alphanumeric word
    QUOTE,  // quoted_term: quoted phrase
    // PHRASE,     // Fuzzy phrase matching, looser than QUOTE
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

        return "[" + typeStr + ": \"" + value + "\"]";
    }
};

#endif  // TOKEN_H_
