enum class TokenType {
    WORD,       // simple_term: alphanumeric word
    PHRASE,     // quoted_term: quoted phrase
    FIELD,      // TITLE or TEXT
    COLON,      // ':'
    OPERATOR,   // AND, OR, NOT, or implicit SPACE
    LPAREN,     // '('
    RPAREN,     // ')'
    EofToken   // end of input
};

struct Token {
    TokenType type;
    std::string value;

    Token(TokenType t, std::string v = "")
        : type(t), value(std::move(v)) {}
};
