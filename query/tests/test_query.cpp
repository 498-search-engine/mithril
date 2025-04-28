#include "../src/Query.h"
#include "TermDictionary.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"

#include <gtest/gtest.h>
#include <string>
#include <random>
#include <sstream>

// Test fixture for Query tests
class QueryTest : public ::testing::Test {
protected:
    // Original index path to restore after tests
    std::string original_index_path;
    std::string test_index_path;
    
    void SetUp() override {
        // Store original index path
        original_index_path = query::QueryConfig::GetIndexPath();
        
        // Create a random index path for testing
        std::random_device rd;
        std::stringstream ss;
        ss << "index_random_" << rd();
        test_index_path = ss.str();

        // Update QueryConfig to use our test index path
        query::QueryConfig::SetIndexPath(test_index_path);
    }
    
    void TearDown() override {
        // Restore original index path
        // const_cast<std::string&>(query::QueryConfig::IndexPath) = original_index_path;
        query::QueryConfig::SetIndexPath(original_index_path);
    }
    
    // Helper to create a Token with the given value
    Token CreateToken(const std::string& value, TokenType type = TokenType::WORD) {
        Token token(type, value);
        return token;
    }
};

// Test that TermQuery can be constructed and evaluated
TEST_F(QueryTest, TermQueryConstruction) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());

    // Create a TermQuery
    TermQuery query(CreateToken("example"), index_file, term_dict, position_index);
    
    // We expect an empty result since the index path is random
    auto results = query.evaluate();
    
    // Verify that evaluating a query with a non-existent index returns empty results
    EXPECT_TRUE(results.empty());
}

// Test that basic Query methods work
TEST_F(QueryTest, BaseQueryMethods) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());

    // The base Query virtual destructor should be callable
    Query* query = new TermQuery(CreateToken("test"), index_file, term_dict, position_index);
    delete query;
    
    // Base Query's Evaluate should return empty vector
    Query base_query;
    auto results = base_query.evaluate();
    EXPECT_TRUE(results.empty());
}

// Test with different token types
TEST_F(QueryTest, DifferentTokenTypes) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    // Test with WORD token
    TermQuery word_query(CreateToken("wordtoken", TokenType::WORD), index_file, term_dict, position_index);
    auto word_results = word_query.evaluate();
    EXPECT_TRUE(word_results.empty());
    
    // Test with QUOTE token
    TermQuery phrase_query(CreateToken("quote token", TokenType::QUOTE), index_file, term_dict, position_index);
    auto phrase_results = phrase_query.evaluate();
    EXPECT_TRUE(phrase_results.empty());
}

// Test with empty token value
TEST_F(QueryTest, EmptyTokenValue) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    TermQuery empty_query(CreateToken(""), index_file, term_dict, position_index);
    auto results = empty_query.evaluate();
    EXPECT_TRUE(results.empty());
}

// Test that QueryConfig path is properly updated
TEST_F(QueryTest, QueryConfigPathUpdated) {
    // Verify that QueryConfig is using our test index path
    EXPECT_EQ(query::QueryConfig::GetIndexPath(), test_index_path);
}

// Test with different random paths
TEST_F(QueryTest, MultipleRandomPaths) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    // Test with first random path
    // const_cast<std::string&>(query::QueryConfig::GetIndexPath() ) = "random_path_1";
    query::QueryConfig::SetIndexPath("random_path_1");
    TermQuery query1(CreateToken("test"), index_file, term_dict, position_index);
    auto results1 = query1.evaluate();
    EXPECT_TRUE(results1.empty());
    
    // Test with second random path
    // const_cast<std::string&>(query::QueryConfig::GetIndexPath()) = "random_path_2";
    query::QueryConfig::SetIndexPath("random_path_2");
    TermQuery query2(CreateToken("test"), index_file, term_dict, position_index);
    auto results2 = query2.evaluate();
    EXPECT_TRUE(results2.empty());
}

// Test with special characters in token
TEST_F(QueryTest, SpecialCharactersInToken) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    TermQuery query(CreateToken("special!@#$%^&*()"), index_file, term_dict, position_index);
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
}

// Test with very long token
TEST_F(QueryTest, VeryLongToken) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    std::string long_token(1000, 'a'); // 1000 'a' characters
    TermQuery query(CreateToken(long_token), index_file, term_dict, position_index);
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
}

// Test for AndQuery
TEST_F(QueryTest, AndQueryEvaluation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    auto left = new TermQuery(CreateToken("term1"), index_file, term_dict, position_index);
    auto right = new TermQuery(CreateToken("term2"), index_file, term_dict, position_index);
    
    AndQuery query(left, right);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
    
    EXPECT_EQ(query.get_type(), "AndQuery");
    
    std::string repr = query.to_string();
    EXPECT_TRUE(repr.find("AND") != std::string::npos);
    EXPECT_TRUE(repr.find("term1") != std::string::npos);
    EXPECT_TRUE(repr.find("term2") != std::string::npos);
}

// Test for OrQuery
TEST_F(QueryTest, OrQueryEvaluation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    auto left = new TermQuery(CreateToken("term1"), index_file, term_dict, position_index);
    auto right = new TermQuery(CreateToken("term2"), index_file, term_dict, position_index);
    
    OrQuery query(left, right);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
    
    EXPECT_EQ(query.get_type(), "OrQuery");
    
    std::string repr = query.to_string();
    EXPECT_TRUE(repr.find("OR") != std::string::npos);
    EXPECT_TRUE(repr.find("term1") != std::string::npos);
    EXPECT_TRUE(repr.find("term2") != std::string::npos);
}

// Test for NotQuery
TEST_F(QueryTest, NotQueryEvaluation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    auto expr = new TermQuery(CreateToken("term"), index_file, term_dict, position_index);
    
    NotQuery query(expr);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
    
    EXPECT_EQ(query.get_type(), "NotQuery");
    
    std::string repr = query.to_string();
    EXPECT_TRUE(repr.find("NOT") != std::string::npos);
    EXPECT_TRUE(repr.find("term") != std::string::npos);
}

// Test for QuoteQuery
TEST_F(QueryTest, QuoteQueryEvaluation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    QuoteQuery query(CreateToken("exact phrase", TokenType::QUOTE), index_file, term_dict, position_index);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
    
    EXPECT_EQ(query.get_type(), "QuoteQuery");
    
    std::string repr = query.to_string();
    EXPECT_TRUE(repr.find("QUOTE") != std::string::npos);
    EXPECT_TRUE(repr.find("exact phrase") != std::string::npos);
}

// Test for PhraseQuery
TEST_F(QueryTest, PhraseQueryEvaluation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    PhraseQuery query(CreateToken("fuzzy phrase", TokenType::PHRASE), index_file, term_dict, position_index);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
    
    EXPECT_EQ(query.get_type(), "PhraseQuery");
    
    std::string repr = query.to_string();
    EXPECT_TRUE(repr.find("PHRASE") != std::string::npos);
    EXPECT_TRUE(repr.find("fuzzy phrase") != std::string::npos);
}

// Test for nested queries (combining different query types)
TEST_F(QueryTest, NestedQueryEvaluation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    // Create a complex query: (term1 AND term2) OR (term3 AND NOT term4)
    
    auto term1 = new TermQuery(CreateToken("term1"), index_file, term_dict, position_index);
    auto term2 = new TermQuery(CreateToken("term2"), index_file, term_dict, position_index);
    auto term3 = new TermQuery(CreateToken("term3"), index_file, term_dict, position_index);
    auto term4 = new TermQuery(CreateToken("term4"), index_file, term_dict, position_index);
    
    auto not_term4 = new NotQuery(term4);
    auto and_left = new AndQuery(term1, term2);
    auto and_right = new AndQuery(term3, not_term4);
    
    OrQuery query(and_left, and_right);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
    
    std::string repr = query.to_string();
    EXPECT_TRUE(repr.find("OR") != std::string::npos);
    EXPECT_TRUE(repr.find("AND") != std::string::npos);
    EXPECT_TRUE(repr.find("NOT") != std::string::npos);
}

// Test for generate_isr method
TEST_F(QueryTest, GenerateISRMethod) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    // Basic query
    Query base_query;
    auto base_isr = base_query.generate_isr();
    EXPECT_EQ(base_isr, nullptr);
    
    // Term query
    TermQuery term_query(CreateToken("test"), index_file, term_dict, position_index);
    auto term_isr = term_query.generate_isr();
    // The ISR might be null since we're using a random test path without real data
    
    // Quote query
    QuoteQuery quote_query(CreateToken("exact phrase", TokenType::QUOTE), index_file, term_dict, position_index);
    auto quote_isr = quote_query.generate_isr();
    // The ISR might be null since we're using a random test path without real data
}

// Test query type identification
TEST_F(QueryTest, QueryTypeIdentification) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    Query base_query;
    EXPECT_EQ(base_query.get_type(), "Query");
    
    TermQuery term_query(CreateToken("term"), index_file, term_dict, position_index);
    EXPECT_EQ(term_query.get_type(), "TermQuery");
    
    auto left = new TermQuery(CreateToken("term1"), index_file, term_dict, position_index);
    auto right = new TermQuery(CreateToken("term2"), index_file, term_dict, position_index);
    
    AndQuery and_query(left, right);
    EXPECT_EQ(and_query.get_type(), "AndQuery");
    
    OrQuery or_query(left, right);
    EXPECT_EQ(or_query.get_type(), "OrQuery");
    
    NotQuery not_query(left);
    EXPECT_EQ(not_query.get_type(), "NotQuery");
    
    QuoteQuery quote_query(CreateToken("exact phrase", TokenType::QUOTE), index_file, term_dict, position_index);
    EXPECT_EQ(quote_query.get_type(), "QuoteQuery");
    
    PhraseQuery phrase_query(CreateToken("fuzzy phrase", TokenType::PHRASE), index_file, term_dict, position_index);
    EXPECT_EQ(phrase_query.get_type(), "PhraseQuery");
}

// Test handling of multiple terms in a quoted phrase
TEST_F(QueryTest, MultiTermQuoteQuery) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    QuoteQuery query(CreateToken("this is a multi word phrase", TokenType::QUOTE), 
                    index_file, term_dict, position_index);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
}

// Test handling of multiple terms in a fuzzy phrase
TEST_F(QueryTest, MultiTermPhraseQuery) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    PhraseQuery query(CreateToken("this is a multi word fuzzy phrase", TokenType::PHRASE), 
                    index_file, term_dict, position_index);
    
    auto results = query.evaluate();
    EXPECT_TRUE(results.empty());
}

// Test with a NULL query in AND/OR/NOT operators
TEST_F(QueryTest, NullOperandHandling) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    auto term = new TermQuery(CreateToken("term"), index_file, term_dict, position_index);
    
    // This should crash/assert if there's no null check
    EXPECT_THROW(AndQuery bad_and(term, nullptr), std::exception);
    EXPECT_THROW(AndQuery bad_and(nullptr, term), std::exception);
    EXPECT_THROW(AndQuery bad_and(nullptr, nullptr), std::exception);
    
    // Testing that NOT with null operand throws
    EXPECT_THROW(NotQuery bad_not(nullptr), std::exception);
}

// Test to_string output format for different queries
TEST_F(QueryTest, QueryStringRepresentation) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    // Term query
    TermQuery term(CreateToken("term"), index_file, term_dict, position_index);
    std::string term_str = term.to_string();
    EXPECT_FALSE(term_str.empty());
    
    // AND query
    auto left = new TermQuery(CreateToken("left"), index_file, term_dict, position_index);
    auto right = new TermQuery(CreateToken("right"), index_file, term_dict, position_index);
    AndQuery and_query(left, right);
    std::string and_str = and_query.to_string();
    EXPECT_TRUE(and_str.find("AND") != std::string::npos);
    EXPECT_TRUE(and_str.find("left") != std::string::npos);
    EXPECT_TRUE(and_str.find("right") != std::string::npos);
}

// Test with unicode/special characters in query terms
TEST_F(QueryTest, UnicodeInQueryTerms) {
    mithril::TermDictionary term_dict(query::QueryConfig::GetIndexPath());
    mithril::PositionIndex position_index(query::QueryConfig::GetIndexPath());
    core::MemMapFile index_file(query::QueryConfig::GetIndexPath());
    
    // Unicode term
    TermQuery unicode_query(CreateToken("résumé"), index_file, term_dict, position_index);
    auto unicode_results = unicode_query.evaluate();
    EXPECT_TRUE(unicode_results.empty());
    
    // Quote with unicode
    QuoteQuery quote_unicode(CreateToken("café au lait", TokenType::QUOTE), 
                           index_file, term_dict, position_index);
    auto quote_results = quote_unicode.evaluate();
    EXPECT_TRUE(quote_results.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
