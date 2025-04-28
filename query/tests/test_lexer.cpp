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

// Additional test cases for test_lexer.cpp

// Test nested expressions with multiple levels of parentheses
TEST_F(LexerTest, ComplexNestedExpressions) {
    Lexer lexer("(TITLE:query AND (TEXT:important OR (URL:example AND NOT DESC:irrelevant)))");
    
    std::vector<TokenType> expected_types = {
        TokenType::LPAREN, TokenType::FIELD, TokenType::COLON, TokenType::WORD, 
        TokenType::OPERATOR, TokenType::LPAREN, TokenType::FIELD, TokenType::COLON, 
        TokenType::WORD, TokenType::OPERATOR, TokenType::LPAREN, TokenType::FIELD, 
        TokenType::COLON, TokenType::WORD, TokenType::OPERATOR, TokenType::OPERATOR, 
        TokenType::FIELD, TokenType::COLON, TokenType::WORD, TokenType::RPAREN, 
        TokenType::RPAREN, TokenType::RPAREN, TokenType::EOFTOKEN
    };
    
    std::vector<std::string> expected_values = {
        "(", "TITLE", ":", "query", "AND", "(", "TEXT", ":", "important", 
        "OR", "(", "URL", ":", "example", "AND", "NOT", "DESC", ":", 
        "irrelevant", ")", ")", ")"
    };
    
    for (size_t i = 0; i < expected_types.size(); i++) {
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, expected_types[i]) << "Failed at token " << i;
        if (i < expected_values.size()) {
            EXPECT_EQ(token.value, expected_values[i]) << "Failed at token " << i;
        }
    }
}

// Test multiple quoted phrases in a complex query
TEST_F(LexerTest, MultipleQuotedPhrases) {
    Lexer lexer("TITLE:\"first phrase\" AND TEXT:\"second phrase\" OR \"standalone phrase\"");
    
    // TITLE
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    // :
    EXPECT_EQ(lexer.NextToken().type, TokenType::COLON);
    // "first phrase"
    Token quote1 = lexer.NextToken();
    EXPECT_EQ(quote1.type, TokenType::QUOTE);
    EXPECT_EQ(quote1.value, "first phrase");
    // AND
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    // TEXT
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    // :
    EXPECT_EQ(lexer.NextToken().type, TokenType::COLON);
    // "second phrase"
    Token quote2 = lexer.NextToken();
    EXPECT_EQ(quote2.type, TokenType::QUOTE);
    EXPECT_EQ(quote2.value, "second phrase");
    // OR
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    // "standalone phrase"
    Token quote3 = lexer.NextToken();
    EXPECT_EQ(quote3.type, TokenType::QUOTE);
    EXPECT_EQ(quote3.value, "standalone phrase");
    // EOF
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test special characters within quoted phrases
TEST_F(LexerTest, QuotedPhrasesWithSpecialChars) {
    Lexer lexer("\"phrase with: symbols! and-punctuation?\"");
    
    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::QUOTE);
    EXPECT_EQ(token.value, "phrase with: symbols! and-punctuation?");
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test all field types
TEST_F(LexerTest, AllFieldTypes) {
    Lexer lexer("TITLE URL ANCHOR DESC TEXT");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::FIELD);
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test operator precedence scenarios
TEST_F(LexerTest, OperatorPrecedence) {
    Lexer lexer("term1 AND term2 OR term3 NOT term4");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::WORD); // term1
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR); // AND
    EXPECT_EQ(lexer.NextToken().type, TokenType::WORD); // term2
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR); // OR
    EXPECT_EQ(lexer.NextToken().type, TokenType::WORD); // term3
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR); // NOT
    EXPECT_EQ(lexer.NextToken().type, TokenType::WORD); // term4
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test mixed case keywords (should be treated as regular words)
TEST_F(LexerTest, MixedCaseKeywords) {
    Lexer lexer("Title Url And Or Not");
    
    for (int i = 0; i < 5; i++) {
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, TokenType::WORD);
    }
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test very long words/tokens
TEST_F(LexerTest, VeryLongTokens) {
    std::string longWord(1000, 'a');
    Lexer lexer(longWord);
    
    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::WORD);
    EXPECT_EQ(token.value, longWord);
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test numbers and alphanumeric tokens
TEST_F(LexerTest, NumericAndAlphanumericTokens) {
    Lexer lexer("123 word123 123word word-123 123-456");
    
    std::vector<std::string> expected = {
        "123", "word123", "123word", "word-123", "123-456"
    };
    
    for (const auto& expected_value : expected) {
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, TokenType::WORD);
        EXPECT_EQ(token.value, expected_value);
    }
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test edge cases for quotes and escaping
TEST_F(LexerTest, QuoteEdgeCases) {
    // Test empty quotes
    Lexer lexer1("\"\"");
    Token empty_quote = lexer1.NextToken();
    EXPECT_EQ(empty_quote.type, TokenType::QUOTE);
    EXPECT_EQ(empty_quote.value, "");
    EXPECT_EQ(lexer1.NextToken().type, TokenType::EOFTOKEN);
    
    // Test quotes with only spaces
    Lexer lexer2("\"   \"");
    Token space_quote = lexer2.NextToken();
    EXPECT_EQ(space_quote.type, TokenType::QUOTE);
    EXPECT_EQ(space_quote.value, "   ");
    EXPECT_EQ(lexer2.NextToken().type, TokenType::EOFTOKEN);
    
    // Test escaped quotes at beginning and end
    Lexer lexer3("\"\\\"quoted\\\"\"");
    Token escaped_quote = lexer3.NextToken();
    EXPECT_EQ(escaped_quote.type, TokenType::QUOTE);
    // The exact expected value depends on how escaping is handled in your lexer
    EXPECT_EQ(lexer3.NextToken().type, TokenType::EOFTOKEN);
}

// Test consecutive operators (which might be an error in actual parsing)
TEST_F(LexerTest, ConsecutiveOperators) {
    Lexer lexer("AND OR NOT");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR);
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test partial operator names
TEST_F(LexerTest, PartialOperatorNames) {
    Lexer lexer("AN AND ORR NOTER NOTAND");
    
    // AN
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::WORD);
    EXPECT_EQ(token1.value, "AN");
    
    // AND
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::OPERATOR);
    EXPECT_EQ(token2.value, "AND");
    
    // ORR
    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::WORD);
    EXPECT_EQ(token3.value, "ORR");
    
    // NOTER
    Token token4 = lexer.NextToken();
    EXPECT_EQ(token4.type, TokenType::WORD);
    EXPECT_EQ(token4.value, "NOTER");
    
    // NOTAND
    Token token5 = lexer.NextToken();
    EXPECT_EQ(token5.type, TokenType::WORD);
    EXPECT_EQ(token5.value, "NOTAND");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test quoted phrases with field specifiers
TEST_F(LexerTest, FieldsWithMultipleQuotes) {
    Lexer lexer("TITLE:\"first\" AND TITLE:\"second\" AND TITLE:\"third\"");
    
    std::vector<TokenType> expected_types = {
        TokenType::FIELD, TokenType::COLON, TokenType::QUOTE,
        TokenType::OPERATOR, TokenType::FIELD, TokenType::COLON,
        TokenType::QUOTE, TokenType::OPERATOR, TokenType::FIELD,
        TokenType::COLON, TokenType::QUOTE, TokenType::EOFTOKEN
    };
    
    std::vector<std::string> expected_values = {
        "TITLE", ":", "first", "AND", "TITLE", ":", "second", "AND", "TITLE", ":", "third"
    };
    
    for (size_t i = 0; i < expected_types.size(); i++) {
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, expected_types[i]) << "Failed at token " << i;
        if (i < expected_values.size()) {
            EXPECT_EQ(token.value, expected_values[i]) << "Failed at token " << i;
        }
    }
}

// Test handling of punctuation in words
TEST_F(LexerTest, PunctuationInWords) {
    Lexer lexer("word. word, word; word's word-dash word_underscore");
    
    std::vector<std::string> expected = {
        "word", "word", "word", "word's", "word-dash", "word_underscore"
    };
    
    for (const auto& expected_value : expected) {
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, TokenType::WORD);
        EXPECT_EQ(token.value, expected_value);
    }
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test token frequency counting
TEST_F(LexerTest, TokenFrequencies) {
    Lexer lexer("word word TITLE:word \"quoted phrase\" \"quoted phrase\"");
    
    auto frequencies = lexer.GetTokenFrequencies();
    
    EXPECT_EQ(frequencies["word"], 3);
    EXPECT_EQ(frequencies["quoted phrase"], 2);
}

// Test single quoted phrases (PHRASE token type)
TEST_F(LexerTest, SingleQuotedPhrases) {
    Lexer lexer("'single quoted phrase' AND 'another phrase'");
    
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::PHRASE);
    EXPECT_EQ(token1.value, "single quoted phrase");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR); // AND
    
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::PHRASE);
    EXPECT_EQ(token2.value, "another phrase");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test for special prefix handling
TEST_F(LexerTest, SpecialPrefixes) {
    Lexer lexer("title:word url:example anchor:link desc:description");
    
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::TITLE);
    EXPECT_EQ(token1.value, "word");
    
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::URL);
    EXPECT_EQ(token2.value, "example");
    
    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::ANCHOR);
    EXPECT_EQ(token3.value, "link");
    
    Token token4 = lexer.NextToken();
    EXPECT_EQ(token4.type, TokenType::DESC);
    EXPECT_EQ(token4.value, "description");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

// Test mixed quoted phrases and special prefixes
TEST_F(LexerTest, MixedQuotesAndPrefixes) {
    Lexer lexer("title:\"quoted title\" AND url:'single quoted url'");
    
    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::TITLE);
    EXPECT_EQ(token1.value, "\"quoted title\"");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::OPERATOR); // AND
    
    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::URL);
    EXPECT_EQ(token2.value, "'single quoted url'");
    
    EXPECT_EQ(lexer.NextToken().type, TokenType::EOFTOKEN);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}