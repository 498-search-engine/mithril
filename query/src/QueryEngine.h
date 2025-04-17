#ifndef QUERYENGINE_H
#define QUERYENGINE_H

#include "DocumentMapReader.h"
#include "Parser.h"
#include "PositionIndex.h"
#include "Query.h"
#include "QueryConfig.h"
#include "TermDictionary.h"

#include <iostream>
#include <memory>
#include <vector>

using namespace mithril;

class QueryEngine {
public:
    QueryEngine(const std::string& index_dir)
        : map_reader_(index_dir), term_dict_(index_dir), position_index_(index_dir) {
        query::QueryConfig::SetIndexPath(index_dir);
        query::QueryConfig::SetMaxDocId(map_reader_.documentCount());

        std::cout << "Query engine initialized\n";
    }

    auto ParseQuery(const std::string& input) -> std::unique_ptr<Query> {
        Parser parser(input, term_dict_, position_index_);
        return std::move(parser.parse());
    }

    std::vector<Token> GetTokens(const std::string& input) {
        Parser parser(input, term_dict_, position_index_);
        return parser.get_tokens();
    }

    std::vector<uint32_t> EvaluateQuery(std::string input) {
        Parser parser(input, term_dict_, position_index_);
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

    mithril::PositionIndex position_index_;

private:
    mithril::DocumentMapReader map_reader_;
    mithril::TermDictionary term_dict_;
};

#endif /* QUERYENGINE_H */
