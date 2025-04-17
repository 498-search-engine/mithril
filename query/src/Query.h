// Define a grammar in here

#ifndef QUERY_H_
#define QUERY_H_

#include <cstdint>
// #include <_types/_uint32_t.h>
#include "NotIndexStreamReader.h"
#include "QueryConfig.h"
#include "TermAND.h"
#include "TermDictionary.h"
#include "TermOR.h"
#include "TermQuote.h"
#include "TermReader.h"
#include "Token.h"
#include "intersect.h"
#include "PositionIndex.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5;


// namespace mithril {


class Query {
public:
    virtual ~Query() {}

    // Evaluates everything in one go
    [[nodiscard]] virtual std::vector<uint32_t> evaluate() const { return {}; }

    // Helps us do some more fine grained stream reading
    virtual uint32_t get_next_doc() const { return 0; }
    virtual bool has_next() const { return false; }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const { return nullptr; };

    // Returns a string representation of the query for debugging/display
    [[nodiscard]] virtual std::string to_string() const { return "Query"; }

    // Optional: Get query type as string
    [[nodiscard]] virtual std::string get_type() const { return "Query"; }

private:
};

class TermQuery : public Query {
public:
    TermQuery(Token token, const core::MemMapFile& index_file,
              mithril::TermDictionary& term_dict, mithril::PositionIndex& position_index)
        : token_(std::move(token)), index_file_(index_file),
          term_dict_(term_dict), position_index_(position_index) {}

    Token get_token() { return token_; }

    std::vector<uint32_t> evaluate() const override {
        mithril::TermReader term(query::QueryConfig::GetIndexPath(), token_.value, index_file_, term_dict_, position_index_);

        std::vector<uint32_t> results;
        results.reserve(MAX_DOCUMENTS);

        while (term.hasNext()) {
            const uint32_t docId = term.currentDocID();
            results.emplace_back(docId);
            term.moveNext();
        }

        return results;
    }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {
        return std::make_unique<mithril::TermReader>(query::QueryConfig::GetIndexPath(), token_.value, index_file_, term_dict_, position_index_);
    }

    [[nodiscard]] std::string to_string() const override { return "TERM(" + token_.value + ")"; }

    [[nodiscard]] std::string get_type() const override { return "TermQuery"; }

private:
    Token token_;
    static constexpr int MAX_DOCUMENTS = 100000;
    const core::MemMapFile& index_file_;
    mithril::TermDictionary& term_dict_;
    mithril::PositionIndex& position_index_;
};


class AndQuery : public Query {
    Query* left_;
    Query* right_;

public:
    AndQuery(Query* left, Query* right) : left_(left), right_(right) {
        if (!left && !right) {
            std::cerr << "Need a left and right query\n";
            exit(1);
        }
    }

    std::vector<uint32_t> evaluate() const override {
        std::vector<uint32_t> left_docs = left_->evaluate();
        std::vector<uint32_t> right_docs = right_->evaluate();
        auto intersected_ids = intersect_simple(left_docs, right_docs);

        return intersected_ids;
    }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
        readers.push_back(std::move(left_->generate_isr()));
        readers.push_back(std::move(right_->generate_isr()));
        return std::make_unique<mithril::TermAND>(std::move(readers));
    }

    [[nodiscard]] std::string to_string() const override {
        return "AND(" + left_->to_string() + ", " + right_->to_string() + ")";
    }

    [[nodiscard]] std::string get_type() const override { return "AndQuery"; }
};

class OrQuery : public Query {
    Query* left_;
    Query* right_;

public:
    OrQuery(Query* left, Query* right) : left_(left), right_(right) {
        if (!left && !right) {
            std::cerr << "Need a left and right query\n";
            exit(1);
        }
    }

    std::vector<uint32_t> evaluate() const override {
        std::vector<uint32_t> left_docs = left_->evaluate();
        std::vector<uint32_t> right_docs = right_->evaluate();
        auto intersected_ids = union_simple(left_docs, right_docs);

        return intersected_ids;
    }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
        readers.push_back(std::move(left_->generate_isr()));
        readers.push_back(std::move(right_->generate_isr()));
        return std::make_unique<mithril::TermOR>(std::move(readers));
    }

    [[nodiscard]] std::string to_string() const override {
        return "OR(" + left_->to_string() + ", " + right_->to_string() + ")";
    }

    [[nodiscard]] std::string get_type() const override { return "OrQuery"; }
};


class NotQuery : public Query {
public:
    NotQuery(Query* expression)
        : expression_(expression),
          not_isr_(std::make_unique<mithril::NotISR>(std::move(expression->generate_isr()),
                                                     query::QueryConfig::GetMaxDocId())) {
        if (!expression) {
            std::cerr << "Need an expression for NOT query\n";
            exit(1);
        }
    }

    std::vector<uint32_t> evaluate() const override {
        // Get all documents that match the expression
        std::vector<uint32_t> expr_docs = expression_->evaluate();
        std::vector<uint32_t> all_docs;
        all_docs.reserve(query::QueryConfig::GetMaxDocId() - expr_docs.size());

        // Generate all document IDs from 0 to max_doc_id
        for (uint32_t i = 0; i < query::QueryConfig::GetMaxDocId(); i++) {
            all_docs.push_back(i);
        }

        // Return documents that are NOT in expr_docs
        std::vector<uint32_t> result;
        size_t expr_idx = 0;

        for (uint32_t doc_id : all_docs) {
            while (expr_idx < expr_docs.size() && expr_docs[expr_idx] < doc_id) {
                expr_idx++;
            }

            if (expr_idx >= expr_docs.size() || expr_docs[expr_idx] != doc_id) {
                result.push_back(doc_id);
            }
        }

        return result;
    }

    [[nodiscard]] std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {
        return std::make_unique<mithril::NotISR>(expression_->generate_isr(), query::QueryConfig::GetMaxDocId());
    }

    [[nodiscard]] std::string to_string() const override { return "NOT(" + expression_->to_string() + ")"; }

    [[nodiscard]] std::string get_type() const override { return "NotQuery"; }

private:
    Query* expression_;
    std::unique_ptr<mithril::NotISR> not_isr_;
};

// class QuoteQuery : public Query {
// public:
//     QuoteQuery
//     ~QuoteQuery() = default;

//     // Evaluates everything in one go
//     [[nodiscard]] std::vector<uint32_t> evaluate() const;

//     // Helps us do some more fine grained stream reading
//     uint32_t get_next_doc() const;
//     bool has_next() const;

//     [[nodiscard]] std::unique_ptr<mithril::IndexStreamReader> generate_isr() const;

//     // Returns a string representation of the query for debugging/display
//     [[nodiscard]] virtual std::string to_string() const;

//     // Optional: Get query type as string
//     [[nodiscard]] virtual std::string get_type() const {
//         return "Quote";
//     }

// private:
// };

// }  // namespace mithril

// class FieldQuery : public Query {
//     // Restricts search to specific fields (TITLE, TEXT)
// };

// class PhraseQuery : public Query {
//     // For exact phrase matching with quotes
//     // Will need to use position information
// };

#endif  // QUERY_H_
