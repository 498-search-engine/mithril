#include "../src/Lexer.h"
#include <gtest/gtest.h>

class LexerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic token recognition
TEST_F(LexerTest, BasicTokens) {
    Lexer lexer("TITLE TEXT AND OR NOT : ( )");
    
    // Check field keywords
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    
    // Check operator keywords
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    
    // Check symbols
    EXPECT_EQ(lexer.NextToken().type, TokenType::COLON);
    EXPECT_EQ(lexer.NextToken().type, TokenType::LPAREN);
    EXPECT_EQ(lexer.NextToken().type, TokenType::RPAREN);
    
    // Should be at end of input
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
    EXPECT_TRUE(lexer.EndOfInput());
}

// Test token values
TEST_F(LexerTest, TokenValues) {
    Lexer lexer("TITLE TEXT AND OR NOT : ( )");
    
    EXPECT_EQ(lexer.NextToken().value, "TITLE");
    EXPECT_EQ(lexer.NextToken().value, "TEXT");
    EXPECT_EQ(lexer.NextToken().value, "AND");
    EXPECT_EQ(lexer.NextToken().value, "OR");
    EXPECT_EQ(lexer.NextToken().value, "NOT");
    EXPECT_EQ(lexer.NextToken().value, ":");
    EXPECT_EQ(lexer.NextToken().value, "(");
    EXPECT_EQ(lexer.NextToken().value, ")");
}

// Test regular words
TEST_F(LexerTest, WordTokens) {
    Lexer lexer("hello world");
    
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::WORD);
    EXPECT_EQ(token1.value, "hello");
    
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::WORD);
    EXPECT_EQ(token2.value, "world");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test quoted phrases
TEST_F(LexerTest, QuotedPhrases) {
    Lexer lexer("\"hello world\" \"another quote\"");
    
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::QUOTE);
    EXPECT_EQ(token1.value, "hello world");
    
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::QUOTE);
    EXPECT_EQ(token2.value, "another quote");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test PeekToken functionality
TEST_F(LexerTest, PeekToken) {
    Lexer lexer("TITLE TEXT");
    
    // Peek first token
    Token peeked1 = lexer.PeekToken();
    EXPECT_EQ(peeked1.type, TokenType::FIELD);
    EXPECT_EQ(peeked1.value, "TITLE");
    
    // Peek again should return same token
    Token peeked2 = lexer.PeekToken();
    EXPECT_EQ(peeked2.type, TokenType::FIELD);
    EXPECT_EQ(peeked2.value, "TITLE");
    
    // Next should consume the token
    Token next1 = lexer.NextToken();
    EXPECT_EQ(next1.type, TokenType::FIELD);
    EXPECT_EQ(next1.value, "TITLE");
    
    // Peek next token
    Token peeked3 = lexer.PeekToken();
    EXPECT_EQ(peeked3.type, TokenType::FIELD);
    EXPECT_EQ(peeked3.value, "TEXT");
    
    // Next should consume the token
    Token next2 = lexer.NextToken();
    EXPECT_EQ(next2.type, TokenType::FIELD);
    EXPECT_EQ(next2.value, "TEXT");
    
    // End of input
    EXPECT_EQ(lexer.PeekToken().type, TokenType::EOFTOKEN);
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test whitespace handling
TEST_F(LexerTest, WhitespaceHandling) {
    Lexer lexer("  TITLE  \t TEXT  \n  ");
    
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::FIELD);
    EXPECT_EQ(token1.value, "TITLE");
    
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::FIELD);
    EXPECT_EQ(token2.value, "TEXT");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test mixed query
TEST_F(LexerTest, MixedQuery) {
    Lexer lexer("TITLE:\"search quote\" AND (TEXT:term OR NOT something)");
    
    // TITLE
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::FIELD);
    EXPECT_EQ(token1.value, "TITLE");
    
    // :
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::COLON);
    
    // "search quote"
    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::QUOTE);
    EXPECT_EQ(token3.value, "search quote");
    
    // AND
    Token token4 = lexer.NextToken();
    EXPECT_EQ(token4.type, TokenType::OPERATOR);
    EXPECT_EQ(token4.value, "AND");
    
    // (
    Token token5 = lexer.NextToken();
    EXPECT_EQ(token5.type, TokenType::LPAREN);
    
    // TEXT
    Token token6 = lexer.NextToken();
    EXPECT_EQ(token6.type, TokenType::FIELD);
    EXPECT_EQ(token6.value, "TEXT");
    
    // :
    Token token7 = lexer.NextToken();
    EXPECT_EQ(token7.type, TokenType::COLON);
    
    // term
    Token token8 = lexer.NextToken();
    EXPECT_EQ(token8.type, TokenType::WORD);
    EXPECT_EQ(token8.value, "term");
    
    // OR
    Token token9 = lexer.NextToken();
    EXPECT_EQ(token9.type, TokenType::OPERATOR);
    EXPECT_EQ(token9.value, "OR");
    
    // NOT
    Token token10 = lexer.NextToken();
    EXPECT_EQ(token10.type, TokenType::OPERATOR);
    EXPECT_EQ(token10.value, "NOT");
    
    // something
    Token token11 = lexer.NextToken();
    EXPECT_EQ(token11.type, TokenType::WORD);
    EXPECT_EQ(token11.value, "something");
    
    // )
    Token token12 = lexer.NextToken();
    EXPECT_EQ(token12.type, TokenType::RPAREN);
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test error handling
TEST_F(LexerTest, ErrorHandling) {
    // Unterminated quoted phrase
    Lexer lexer1("\"unterminated");
    EXPECT_THROW(lexer1.NextToken(), std::runtime_error);
    
    // Unexpected character
    Lexer lexer2("$unexpected");
    EXPECT_THROW(lexer2.NextToken(), std::runtime_error);
}

// Test empty input
TEST_F(LexerTest, EmptyInput) {
    Lexer lexer("");
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
    EXPECT_TRUE(lexer.EndOfInput());
}

// Test case insensitivity for keywords
TEST_F(LexerTest, KeywordCaseSensitivity) {
    Lexer lexer("title text and or not");
    
    // These should be treated as regular words, not keywords
    for (int i = 0; i < 5; i++) {
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, TokenType::WORD);
    }
}

TEST_F(LexerTest, QuotedPhraseWithInnerQuote) {
    Lexer lexer("\"hello \\\"world\\\" again\"");
    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::QUOTE);
    EXPECT_EQ(token.value, "hello \\\"world\\\" again"); // Or actual unescaped
}

TEST_F(LexerTest, OperatorSurroundedByInsaneWhitespace) {
    Lexer lexer("a     AND     b");
    Token a = lexer.NextToken();
    Token op = lexer.NextToken();
    Token b = lexer.NextToken();
    EXPECT_EQ(a.type, TokenType::WORD);
    EXPECT_EQ(op.type, TokenType::OPERATOR);
    EXPECT_EQ(op.value, "AND");
    EXPECT_EQ(b.type, TokenType::WORD);
}

TEST_F(LexerTest, WeirdlySpacedQuery) {
    Lexer lexer(" (  TITLE : \"x\"  OR   TEXT : y ) ");
    std::vector<TokenType> expected = {
        TokenType::LPAREN, TokenType::FIELD, TokenType::COLON, TokenType::QUOTE,
        TokenType::OPERATOR, TokenType::FIELD, TokenType::COLON, TokenType::WORD,
        TokenType::RPAREN, TokenType::EOFTOKEN
    };
    for (auto type : expected) {
        EXPECT_EQ(lexer.NextToken().type, type);
    }
}

TEST_F(LexerTest, MultipleEOFAccessesAreStable) {
    Lexer lexer("chatbot");
    lexer.NextToken(); // WORD
    lexer.NextToken(); // EOF
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
    }
}

TEST_F(LexerTest, InvalidSymbolsAreRejected) {
    Lexer lexer("hello @world");
    Token word = lexer.NextToken();
    EXPECT_EQ(word.type, TokenType::WORD);
    EXPECT_THROW(lexer.NextToken(), std::runtime_error);
}

TEST_F(LexerTest, QuotedPhraseWithLineBreakFails) {
    Lexer lexer("\"hello\nworld\"");
    EXPECT_THROW(lexer.NextToken(), std::runtime_error);
}

TEST_F(LexerTest, PeekAtEOFStaysEOF) {
    Lexer lexer("word");
    lexer.NextToken(); // consume word
    for (int i = 0; i < 3; ++i) {
        Token peeked = lexer.PeekToken();
        EXPECT_EQ(peeked.type, TokenType::EOFTOKEN);
    }
}

TEST_F(LexerTest, FieldColonMisuse) {
    Lexer lexer("TITLE::something");
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::COLON);
    EXPECT_EQ(lexer.NextToken().type, TokenType::COLON);
    EXPECT_EQ(lexer.NextToken().type, TokenType::WORD);
    EXPECT_EQ(lexer.EndOfInput(), true);
    // EXPECT_THROW(lexer.NextToken(), std::runtime_error);
}

TEST_F(LexerTest, TokensWithoutSpace) {
    Lexer lexer("TITLE:\"foo\"ANDbar");
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::COLON);
    EXPECT_EQ(lexer.NextToken().type, TokenType::QUOTE);
    EXPECT_EQ(lexer.NextToken().type, TokenType::WORD); // ANDbar is not "AND"
}

TEST_F(LexerTest, StressTestManyTokens) {
    std::string input;
    for (int i = 0; i < 1000; ++i) {
        input += "word ";
    }
    Lexer lexer(input);
    for (int i = 0; i < 1000; ++i) {
        Token t = lexer.NextToken();
        EXPECT_EQ(t.type, TokenType::WORD);
        EXPECT_EQ(t.value, "word");
    }
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}