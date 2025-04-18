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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
