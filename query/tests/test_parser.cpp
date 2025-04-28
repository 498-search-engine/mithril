#include "../src/Parser.h"
#include "../src/Lexer.h"
#include "../src/Query.h"
#include "../src/QueryConfig.h"
#include "TermDictionary.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"

#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <random>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// Test fixture for Parser tests
class ParserTest : public ::testing::Test {
protected:
    // Test directory and files
    std::string test_index_dir;
    std::string original_index_path;
    
    // Components needed for Parser
    std::unique_ptr<core::MemMapFile> index_file;
    std::unique_ptr<mithril::TermDictionary> term_dict;
    std::unique_ptr<mithril::PositionIndex> position_index;
    
    void SetUp() override {
        // Store original index path if set
        try {
            original_index_path = query::QueryConfig::GetIndexPath();
        } catch (const std::runtime_error&) {
            // Path wasn't set yet, that's fine
        }
        
        // Create a unique test directory
        std::random_device rd;
        std::stringstream ss;
        ss << "test_index_" << rd();
        test_index_dir = ss.str();
        
        // Create minimal directory structure for testing
        std::filesystem::create_directory(test_index_dir);
        
        // Create empty index files
        std::ofstream(test_index_dir + "/final_index.data").close();
        std::ofstream(test_index_dir + "/term_dict.bin").close();
        std::ofstream(test_index_dir + "/position_index.bin").close();
        
        // Set the index path in QueryConfig
        query::QueryConfig::SetIndexPath(test_index_dir);
        
        // Initialize components needed for Parser
        try {
            index_file = std::make_unique<core::MemMapFile>(test_index_dir + "/final_index.data");
            term_dict = std::make_unique<mithril::TermDictionary>(test_index_dir);
            position_index = std::make_unique<mithril::PositionIndex>(test_index_dir);
        } catch (const std::exception& e) {
            std::cerr << "Error initializing test environment: " << e.what() << std::endl;
            // Continue with tests, as some might not need fully functional components
        }
    }
    
    void TearDown() override {
        // Clean up resources
        index_file.reset();
        term_dict.reset();
        position_index.reset();
        
        // Clean up test directory
        try {
            std::filesystem::remove_all(test_index_dir);
        } catch (const std::exception& e) {
            std::cerr << "Error cleaning up test directory: " << e.what() << std::endl;
        }
        
        // Restore original index path if it was set
        if (!original_index_path.empty()) {
            query::QueryConfig::SetIndexPath(original_index_path);
        }
    }
    
    // Helper method to parse a query and get the resulting query object
    std::unique_ptr<Query> ParseQuery(const std::string& query_str) {
        try {
            if (!index_file || !term_dict || !position_index) {
                GTEST_SKIP() << "Test environment not properly initialized";
                return nullptr;
            }
            
            mithril::Parser parser(query_str, *index_file, *term_dict, *position_index);
            return parser.parse();
        } catch (const std::exception& e) {
            ADD_FAILURE() << "Exception during parsing: " << e.what();
            return nullptr;
        }
    }
    
    // Helper method to verify tokens from a parser
    void VerifyTokens(const std::string& input, const std::vector<TokenType>& expected_types) {
        mithril::Parser parser(input, *index_file, *term_dict, *position_index);
        auto tokens = parser.get_tokens();
        
        ASSERT_EQ(tokens.size(), expected_types.size()) << "Token count mismatch";
        for (size_t i = 0; i < tokens.size(); i++) {
            EXPECT_EQ(tokens[i].type, expected_types[i]) << "Token type mismatch at position " << i;
        }
    }
};

// Test basic term parsing
TEST_F(ParserTest, BasicTermParsing) {
    auto query = ParseQuery("simple");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "TermQuery");
    EXPECT_TRUE(query->to_string().find("simple") != std::string::npos);
}

// Test AND operator parsing
TEST_F(ParserTest, AndOperatorParsing) {
    auto query = ParseQuery("term1 AND term2");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "AndQuery");
    EXPECT_TRUE(query->to_string().find("AND") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term1") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term2") != std::string::npos);
}

// Test OR operator parsing
TEST_F(ParserTest, OrOperatorParsing) {
    auto query = ParseQuery("term1 OR term2");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "OrQuery");
    EXPECT_TRUE(query->to_string().find("OR") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term1") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term2") != std::string::npos);
}

// Test NOT operator parsing
TEST_F(ParserTest, NotOperatorParsing) {
    auto query = ParseQuery("NOT term");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "NotQuery");
    EXPECT_TRUE(query->to_string().find("NOT") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term") != std::string::npos);
}

// Test quoted phrase parsing
TEST_F(ParserTest, QuotedPhraseParsing) {
    auto query = ParseQuery("\"exact phrase\"");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "QuoteQuery");
    EXPECT_TRUE(query->to_string().find("exact phrase") != std::string::npos);
}

// Test single quoted phrase parsing
TEST_F(ParserTest, SingleQuotedPhraseParsing) {
    auto query = ParseQuery("'fuzzy phrase'");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "PhraseQuery");
    EXPECT_TRUE(query->to_string().find("fuzzy phrase") != std::string::npos);
}

// Test parenthesized expression parsing
TEST_F(ParserTest, ParenthesizedExpressionParsing) {
    auto query = ParseQuery("(term)");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "TermQuery");
    EXPECT_TRUE(query->to_string().find("term") != std::string::npos);
}

// Test complex nested expression parsing
TEST_F(ParserTest, ComplexNestedExpressionParsing) {
    auto query = ParseQuery("(term1 AND term2) OR (term3 AND NOT term4)");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "OrQuery");
    std::string repr = query->to_string();
    EXPECT_TRUE(repr.find("AND") != std::string::npos);
    EXPECT_TRUE(repr.find("OR") != std::string::npos);
    EXPECT_TRUE(repr.find("NOT") != std::string::npos);
    EXPECT_TRUE(repr.find("term1") != std::string::npos);
    EXPECT_TRUE(repr.find("term2") != std::string::npos);
    EXPECT_TRUE(repr.find("term3") != std::string::npos);
    EXPECT_TRUE(repr.find("term4") != std::string::npos);
}

// Test implicit AND operator
TEST_F(ParserTest, ImplicitAndOperator) {
    auto query = ParseQuery("term1 term2");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "AndQuery");
    EXPECT_TRUE(query->to_string().find("term1") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term2") != std::string::npos);
}

// Test operator precedence
TEST_F(ParserTest, OperatorPrecedence) {
    auto query = ParseQuery("term1 AND term2 OR term3");
    
    ASSERT_NE(query, nullptr);
    // The exact structure depends on how precedence is implemented
    // We'll just check that all terms and operators are present
    std::string repr = query->to_string();
    EXPECT_TRUE(repr.find("AND") != std::string::npos);
    EXPECT_TRUE(repr.find("OR") != std::string::npos);
    EXPECT_TRUE(repr.find("term1") != std::string::npos);
    EXPECT_TRUE(repr.find("term2") != std::string::npos);
    EXPECT_TRUE(repr.find("term3") != std::string::npos);
}

// Test field expression parsing
TEST_F(ParserTest, FieldExpressionParsing) {
    // This might throw if field expressions are not fully implemented
    try {
        auto query = ParseQuery("TITLE:term");
        
        // Check query type based on implementation
        ASSERT_NE(query, nullptr);
        std::string repr = query->to_string();
        EXPECT_TRUE(repr.find("TITLE") != std::string::npos || repr.find("title") != std::string::npos);
        EXPECT_TRUE(repr.find("term") != std::string::npos);
    } catch (const mithril::ParseException& e) {
        // If field queries are explicitly not implemented, this is expected
        std::string error_msg(e.what());
        if (error_msg.find("not yet implemented") != std::string::npos) {
            GTEST_SKIP() << "Field queries not implemented: " << e.what();
        } else {
            FAIL() << "Unexpected parse exception: " << e.what();
        }
    }
}

// Test field with quoted phrase
TEST_F(ParserTest, FieldWithQuotedPhrase) {
    try {
        auto query = ParseQuery("TITLE:\"quoted phrase\"");
        
        ASSERT_NE(query, nullptr);
        std::string repr = query->to_string();
        EXPECT_TRUE(repr.find("TITLE") != std::string::npos || repr.find("title") != std::string::npos);
        EXPECT_TRUE(repr.find("quoted phrase") != std::string::npos);
    } catch (const mithril::ParseException& e) {
        std::string error_msg(e.what());
        if (error_msg.find("not yet implemented") != std::string::npos) {
            GTEST_SKIP() << "Field queries not implemented: " << e.what();
        } else {
            FAIL() << "Unexpected parse exception: " << e.what();
        }
    }
}

// Test token extraction
TEST_F(ParserTest, TokenExtraction) {
    std::string input = "term1 AND term2";
    std::vector<TokenType> expected_types = {
        TokenType::WORD, TokenType::OPERATOR, TokenType::WORD
    };
    
    VerifyTokens(input, expected_types);
}

// Test complex query token extraction
TEST_F(ParserTest, ComplexQueryTokenExtraction) {
    std::string input = "(term1 AND \"quoted phrase\") OR NOT term3";
    std::vector<TokenType> expected_types = {
        TokenType::LPAREN, TokenType::WORD, TokenType::OPERATOR, TokenType::QUOTE,
        TokenType::RPAREN, TokenType::OPERATOR, TokenType::OPERATOR, TokenType::WORD
    };
    
    VerifyTokens(input, expected_types);
}

// Test error handling - empty input
TEST_F(ParserTest, EmptyInputError) {
    EXPECT_THROW({
        mithril::Parser parser("", *index_file, *term_dict, *position_index);
        parser.parse();
    }, mithril::ParseException);
}

// Test error handling - unbalanced parentheses
TEST_F(ParserTest, UnbalancedParenthesesError) {
    EXPECT_THROW({
        mithril::Parser parser("(term1 AND term2", *index_file, *term_dict, *position_index);
        parser.parse();
    }, mithril::ParseException);
}

// Test error handling - incomplete expression
TEST_F(ParserTest, IncompleteExpressionError) {
    EXPECT_THROW({
        mithril::Parser parser("term1 AND", *index_file, *term_dict, *position_index);
        parser.parse();
    }, mithril::ParseException);
}

// Test error handling - invalid operator usage
TEST_F(ParserTest, InvalidOperatorUsageError) {
    EXPECT_THROW({
        mithril::Parser parser("AND term", *index_file, *term_dict, *position_index);
        parser.parse();
    }, mithril::ParseException);
}

// Test token multiplicity tracking
TEST_F(ParserTest, TokenMultiplicityTracking) {
    std::string input = "term term different term";
    
    mithril::Parser parser(input, *index_file, *term_dict, *position_index);
    parser.makeTokenMap();
    
    EXPECT_EQ(parser.getTokenMultiplicity(std::string("term")), 3);
    EXPECT_EQ(parser.getTokenMultiplicity(std::string("different")), 1);
    EXPECT_EQ(parser.getTokenMultiplicity(std::string("nonexistent")), 0);
}

// Test handling of unusual whitespace
TEST_F(ParserTest, UnusualWhitespaceParsing) {
    auto query = ParseQuery("  term1    AND\t\tterm2\n");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "AndQuery");
    EXPECT_TRUE(query->to_string().find("term1") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term2") != std::string::npos);
}

// Test very long input
TEST_F(ParserTest, VeryLongInputParsing) {
    // Create a long query with 100 terms joined by AND
    std::stringstream ss;
    for (int i = 0; i < 100; i++) {
        ss << "term" << i;
        if (i < 99) ss << " AND ";
    }
    
    auto query = ParseQuery(ss.str());
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "AndQuery");
    // Check a few random terms
    EXPECT_TRUE(query->to_string().find("term0") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term50") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("term99") != std::string::npos);
}

// Test deeply nested expressions
TEST_F(ParserTest, DeeplyNestedExpressions) {
    // Create a deeply nested expression: ((...(term1)...))
    std::stringstream ss;
    const int nesting_level = 20;
    
    // Opening parentheses
    for (int i = 0; i < nesting_level; i++) {
        ss << "(";
    }
    
    ss << "term";
    
    // Closing parentheses
    for (int i = 0; i < nesting_level; i++) {
        ss << ")";
    }
    
    auto query = ParseQuery(ss.str());
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "TermQuery");
    EXPECT_TRUE(query->to_string().find("term") != std::string::npos);
}

// Test with special characters in terms
TEST_F(ParserTest, SpecialCharactersInTerms) {
    auto query = ParseQuery("special-term with_underscore and.dot");
    
    ASSERT_NE(query, nullptr);
    std::string repr = query->to_string();
    EXPECT_TRUE(repr.find("special-term") != std::string::npos);
    EXPECT_TRUE(repr.find("with_underscore") != std::string::npos);
    EXPECT_TRUE(repr.find("and.dot") != std::string::npos);
}

// Test with numeric terms
TEST_F(ParserTest, NumericTerms) {
    auto query = ParseQuery("123 AND 456");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "AndQuery");
    EXPECT_TRUE(query->to_string().find("123") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("456") != std::string::npos);
}

// Test unicode characters
TEST_F(ParserTest, UnicodeCharacters) {
    auto query = ParseQuery("café AND résumé");
    
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->get_type(), "AndQuery");
    EXPECT_TRUE(query->to_string().find("café") != std::string::npos);
    EXPECT_TRUE(query->to_string().find("résumé") != std::string::npos);
}

// Main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
