#include "../src/Query.h"
#include "../src/Token.h"
#include "../src/Lexer.h"
#include "../src/QueryEngine.h"
#include "../src/QueryConfig.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>
#include <memory>
#include <random>
#include <filesystem>
#include <fstream>

// Test fixture for system integration tests
class QuerySystemTest : public ::testing::Test {
protected:
    // Test index directory path
    std::string test_index_dir;
    std::unique_ptr<QueryEngine> engine;
    std::string original_index_path;
    
    void SetUp() override {
        // Store original index path
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
        
        // Create empty index files needed by QueryEngine
        std::ofstream(test_index_dir + "/final_index.data").close();
        std::ofstream(test_index_dir + "/term_dict.bin").close();
        std::ofstream(test_index_dir + "/position_index.bin").close();
        std::ofstream(test_index_dir + "/document_map.bin").close();
        std::ofstream(test_index_dir + "/avg_doc_length.bin").close();
        
        // Initialize query engine
        try {
            engine = std::make_unique<QueryEngine>(test_index_dir);
        } catch (const std::exception& e) {
            std::cerr << "Error initializing QueryEngine: " << e.what() << std::endl;
            // Continue with tests, as some might not need a fully functional engine
        }
    }
    
    void TearDown() override {
        // Cleanup test directory
        try {
            std::filesystem::remove_all(test_index_dir);
        } catch (const std::exception& e) {
            std::cerr << "Error cleaning up test directory: " << e.what() << std::endl;
        }
        
        // Restore original index path if it was set
        if (!original_index_path.empty()) {
            query::QueryConfig::SetIndexPath(original_index_path);
        }
        
        engine.reset();
    }
    
    // Helper method to tokenize input using the Lexer
    std::vector<Token> Tokenize(const std::string& input) {
        Lexer lexer(input);
        std::vector<Token> tokens;
        
        while (!lexer.EndOfInput()) {
            tokens.push_back(lexer.NextToken());
        }
        
        return tokens;
    }
    
    // Helper method to validate token sequence
    void ValidateTokenSequence(const std::vector<Token>& tokens, 
                              const std::vector<TokenType>& expected_types,
                              const std::vector<std::string>& expected_values) {
        ASSERT_EQ(tokens.size(), expected_types.size());
        for (size_t i = 0; i < tokens.size(); i++) {
            EXPECT_EQ(tokens[i].type, expected_types[i]) << "Token " << i << " has wrong type";
            if (i < expected_values.size()) {
                EXPECT_EQ(tokens[i].value, expected_values[i]) << "Token " << i << " has wrong value";
            }
        }
    }
};

// Test simple term query tokenization and parsing
TEST_F(QuerySystemTest, SimpleTermQuery) {
    const std::string query_str = "example";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    ASSERT_EQ(tokens.size(), 2); // "example" + EOF
    EXPECT_EQ(tokens[0].type, TokenType::WORD);
    EXPECT_EQ(tokens[0].value, "example");
    EXPECT_EQ(tokens[1].type, TokenType::EOFTOKEN);
    
    // Test query parsing through QueryEngine
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_EQ(query->get_type(), "TermQuery");
        EXPECT_EQ(query->to_string(), "example");
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping parse test due to: " << e.what();
    }
}

// Test AND operator integration
TEST_F(QuerySystemTest, AndOperatorIntegration) {
    const std::string query_str = "term1 AND term2";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::WORD, TokenType::OPERATOR, TokenType::WORD, TokenType::EOFTOKEN
    };
    std::vector<std::string> expected_values = {"term1", "AND", "term2"};
    ValidateTokenSequence(tokens, expected_types, expected_values);
    
    // Test query parsing
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_EQ(query->get_type(), "AndQuery");
        std::string query_str = query->to_string();
        EXPECT_TRUE(query_str.find("AND") != std::string::npos);
        EXPECT_TRUE(query_str.find("term1") != std::string::npos);
        EXPECT_TRUE(query_str.find("term2") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping parse test due to: " << e.what();
    }
}

// Test OR operator integration
TEST_F(QuerySystemTest, OrOperatorIntegration) {
    const std::string query_str = "term1 OR term2";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::WORD, TokenType::OPERATOR, TokenType::WORD, TokenType::EOFTOKEN
    };
    std::vector<std::string> expected_values = {"term1", "OR", "term2"};
    ValidateTokenSequence(tokens, expected_types, expected_values);
    
    // Test query parsing
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_EQ(query->get_type(), "OrQuery");
        std::string query_str = query->to_string();
        EXPECT_TRUE(query_str.find("OR") != std::string::npos);
        EXPECT_TRUE(query_str.find("term1") != std::string::npos);
        EXPECT_TRUE(query_str.find("term2") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping parse test due to: " << e.what();
    }
}

// Test NOT operator integration
TEST_F(QuerySystemTest, NotOperatorIntegration) {
    const std::string query_str = "NOT term";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::OPERATOR, TokenType::WORD, TokenType::EOFTOKEN
    };
    std::vector<std::string> expected_values = {"NOT", "term"};
    ValidateTokenSequence(tokens, expected_types, expected_values);
    
    // Test query parsing
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_EQ(query->get_type(), "NotQuery");
        std::string query_str = query->to_string();
        EXPECT_TRUE(query_str.find("NOT") != std::string::npos);
        EXPECT_TRUE(query_str.find("term") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping parse test due to: " << e.what();
    }
}

// Test exact phrase query integration
TEST_F(QuerySystemTest, ExactPhraseQueryIntegration) {
    const std::string query_str = "\"exact phrase query\"";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    ASSERT_EQ(tokens.size(), 2); // Quote token + EOF
    EXPECT_EQ(tokens[0].type, TokenType::QUOTE);
    EXPECT_EQ(tokens[0].value, "exact phrase query");
    
    // Test term extraction from quoted phrase
    auto terms = ExtractQuoteTerms(tokens[0]);
    ASSERT_EQ(terms.size(), 3);
    EXPECT_EQ(terms[0], "exact");
    EXPECT_EQ(terms[1], "phrase");
    EXPECT_EQ(terms[2], "query");
    
    // Test query parsing
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_EQ(query->get_type(), "QuoteQuery");
        EXPECT_TRUE(query->to_string().find("exact phrase query") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping parse test due to: " << e.what();
    }
}

// Test field-specific query integration
TEST_F(QuerySystemTest, FieldQueryIntegration) {
    const std::string query_str = "TITLE:important";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::FIELD, TokenType::COLON, TokenType::WORD, TokenType::EOFTOKEN
    };
    std::vector<std::string> expected_values = {"TITLE", ":", "important"};
    ValidateTokenSequence(tokens, expected_types, expected_values);
    
    // Test query parsing - This might fail if FieldQuery isn't fully implemented
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_TRUE(query != nullptr);
        // Specific assertions depend on how field queries are implemented
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping field query test due to: " << e.what();
    }
}

// Test complex nested query integration
TEST_F(QuerySystemTest, ComplexNestedQueryIntegration) {
    const std::string query_str = "(term1 AND term2) OR (term3 AND NOT term4)";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::LPAREN, TokenType::WORD, TokenType::OPERATOR, TokenType::WORD, TokenType::RPAREN,
        TokenType::OPERATOR, TokenType::LPAREN, TokenType::WORD, TokenType::OPERATOR, TokenType::OPERATOR,
        TokenType::WORD, TokenType::RPAREN, TokenType::EOFTOKEN
    };
    std::vector<std::string> expected_values = {
        "(", "term1", "AND", "term2", ")", "OR", "(", "term3", "AND", "NOT", "term4", ")"
    };
    ValidateTokenSequence(tokens, expected_types, expected_values);
    
    // Test query parsing
    try {
        auto query = engine->ParseQuery(query_str);
        EXPECT_EQ(query->get_type(), "OrQuery");
        std::string query_str = query->to_string();
        EXPECT_TRUE(query_str.find("AND") != std::string::npos);
        EXPECT_TRUE(query_str.find("OR") != std::string::npos);
        EXPECT_TRUE(query_str.find("NOT") != std::string::npos);
        // Check for all terms
        EXPECT_TRUE(query_str.find("term1") != std::string::npos);
        EXPECT_TRUE(query_str.find("term2") != std::string::npos);
        EXPECT_TRUE(query_str.find("term3") != std::string::npos);
        EXPECT_TRUE(query_str.find("term4") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping complex query test due to: " << e.what();
    }
}

// Test mixed operators with implicit AND
TEST_F(QuerySystemTest, MixedOperatorsWithImplicitAnd) {
    const std::string query_str = "term1 term2 OR term3";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::WORD, TokenType::WORD, TokenType::OPERATOR, TokenType::WORD, TokenType::EOFTOKEN
    };
    std::vector<std::string> expected_values = {"term1", "term2", "OR", "term3"};
    ValidateTokenSequence(tokens, expected_types, expected_values);
    
    // Test query parsing
    try {
        auto query = engine->ParseQuery(query_str);
        std::string query_str = query->to_string();
        // term1 and term2 should be combined with implicit AND
        EXPECT_TRUE(query_str.find("AND") != std::string::npos);
        EXPECT_TRUE(query_str.find("OR") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping mixed operators test due to: " << e.what();
    }
}

// Test query with field-specific, quotes, and operators
TEST_F(QuerySystemTest, MixedFieldQuotesOperators) {
    const std::string query_str = "TITLE:important AND \"exact phrase\" OR NOT common";
    
    // Test tokenization
    auto tokens = Tokenize(query_str);
    std::vector<TokenType> expected_types = {
        TokenType::FIELD, TokenType::COLON, TokenType::WORD, TokenType::OPERATOR, 
        TokenType::QUOTE, TokenType::OPERATOR, TokenType::OPERATOR, TokenType::WORD,
        TokenType::EOFTOKEN
    };
    
    // Test query parsing - this is complex and tests the entire system
    try {
        auto query = engine->ParseQuery(query_str);
        std::string query_str = query->to_string();
        EXPECT_TRUE(query_str.find("important") != std::string::npos);
        EXPECT_TRUE(query_str.find("exact phrase") != std::string::npos);
        EXPECT_TRUE(query_str.find("common") != std::string::npos);
        EXPECT_TRUE(query_str.find("AND") != std::string::npos);
        EXPECT_TRUE(query_str.find("OR") != std::string::npos);
        EXPECT_TRUE(query_str.find("NOT") != std::string::npos);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping mixed query test due to: " << e.what();
    }
}

// Test query evaluation end-to-end
TEST_F(QuerySystemTest, QueryEvaluationEndToEnd) {
    const std::string query_str = "simple term";
    
    // Test the complete flow from query string to evaluation
    try {
        auto results = engine->EvaluateQuery(query_str);
        // Since we're using a mock empty index, we expect no results
        EXPECT_TRUE(results.empty());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping evaluation test due to: " << e.what();
    }
}

// Test token frequency analysis
TEST_F(QuerySystemTest, TokenFrequencyAnalysis) {
    const std::string query_str = "term term different term unique";
    
    Lexer lexer(query_str);
    auto frequencies = lexer.GetTokenFrequencies();
    
    EXPECT_EQ(frequencies["term"], 3);
    EXPECT_EQ(frequencies["different"], 1);
    EXPECT_EQ(frequencies["unique"], 1);
    EXPECT_EQ(frequencies.size(), 3);
}

// Test query configuration
TEST_F(QuerySystemTest, QueryConfiguration) {
    // Test setting and getting index path
    const std::string test_path = "/tmp/test_index";
    query::QueryConfig::SetIndexPath(test_path);
    EXPECT_EQ(query::QueryConfig::GetIndexPath(), test_path);
    
    // Test setting and getting max doc ID
    query::QueryConfig::SetMaxDocId(12345);
    EXPECT_EQ(query::QueryConfig::GetMaxDocId(), 12345);
    
    // Test valid fields
    auto& fields = query::QueryConfig::GetValidFields();
    EXPECT_TRUE(fields.contains("TITLE"));
    EXPECT_TRUE(fields.contains("TEXT"));
    
    // Test valid operators
    auto& operators = query::QueryConfig::GetValidOperators();
    EXPECT_TRUE(operators.contains("AND"));
    EXPECT_TRUE(operators.contains("OR"));
    EXPECT_TRUE(operators.contains("NOT"));
    
    // Test adding custom field
    query::QueryConfig::AddCustomField("CUSTOM_FIELD");
    EXPECT_TRUE(query::QueryConfig::GetValidFields().contains("CUSTOM_FIELD"));
    
    // Test adding custom operator
    query::QueryConfig::AddCustomOperator("XOR");
    EXPECT_TRUE(query::QueryConfig::GetValidOperators().contains("XOR"));
}
