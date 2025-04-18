#ifndef QUERYENGINE_H
#define QUERYENGINE_H

#include "DocumentMapReader.h"
#include "Parser.h"
#include "Query.h"
#include "QueryConfig.h"
#include "TermDictionary.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"

#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>

using namespace mithril;

class QueryEngine {
public:
    QueryEngine(const std::string& index_dir)
        : map_reader_(index_dir), index_file_(index_dir),
          term_dict_(index_dir), position_index_(index_dir)
    {
        query::QueryConfig::SetIndexPath(index_dir);
        query::QueryConfig::SetMaxDocId(map_reader_.documentCount());

        std::cout << "Query engine initialized\n";
    }

    auto ParseQuery(const std::string& input) -> std::unique_ptr<Query> {
        Parser parser(input, index_file_, term_dict_, position_index_);
        return std::move(parser.parse());
    }

    static std::vector<std::pair<std::string, int>> get_alL_terms(std::string query){
        std::vector<Token> tokens = GetTokens(query);
        std::vector<std::pair<std::string, int>> term_counts;
        std::unordered_map<std::string, int> term_map;
        
        // Track if we're in a NOT operation
        bool in_not = false;
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            const Token& token = tokens[i];
            
            // Check for NOT operator
            if (token.type == TokenType::OPERATOR && token.value == "NOT") {
                in_not = true;
                continue;
            }
            
            // Skip terms after NOT
            if (in_not) {
                in_not = false;
                continue;
            }
            
            // Process WORD and QUOTE tokens
            if (token.type == TokenType::WORD) {
                term_map[token.value]++;
            } else if (token.type == TokenType::QUOTE) {
                // Split quoted phrases into individual terms
                std::vector<std::string> quote_terms = extract_quote_terms(token);
                for (const auto& term : quote_terms) {
                    term_map[term]++;
                }
            }
        }
        
        // Convert map to vector of pairs
        for (const auto& [term, count] : term_map) {
            term_counts.emplace_back(term, count);
        }
        
        return term_counts;
    }

    std::vector<Token> GetTokens(const std::string& input) {
        Parser parser(input, index_file_, term_dict_, position_index_);
        return parser.get_tokens();
    }

    std::vector<uint32_t> EvaluateQuery(std::string input) {
        Parser parser(input, index_file_, term_dict_, position_index_);
        auto queryTree = parser.parse();
        if (!queryTree) {
            std::cerr << "Failed to parse query: " << input << std::endl;
            return {};
        }
        return queryTree->evaluate();
    }

    void DisplayTokens(const std::vector<Token>& tokens) const {
        std::cout << "Tokens:" << std::endl;
        for (size_t i = 0; i < tokens.size(); ++i) {
            std::cout << "  " << i + 1 << ": " << tokens[i].toString() << std::endl;
        }
    }

    void DisplayResults(const std::vector<uint32_t>& results, size_t max_display = 10) const {
        std::cout << "Query returned " << results.size() << " results." << std::endl;
        if (!results.empty()) {
            std::cout << "First " << std::min(max_display, results.size()) << " document IDs:" << std::endl;
            for (size_t i = 0; i < std::min(max_display, results.size()); ++i) {
                std::cout << "  " << results[i];
                if (i < std::min(max_display, results.size()) - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        }
    }

    std::optional<data::Document> GetDocument(uint32_t doc_id) const { return map_reader_.getDocument(doc_id); }
    DocInfo GetDocumentInfo(uint32_t doc_id) const { return map_reader_.getDocInfo(doc_id); }

private:
    mithril::DocumentMapReader map_reader_;
    core::MemMapFile index_file_;
    mithril::TermDictionary term_dict_;
    mithril::PositionIndex position_index_;
};

#endif /* QUERYENGINE_H */
