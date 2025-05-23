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
#include "TermPhrase.h"
#include "TermReader.h"
#include "Token.h"
#include "intersect.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"
#include "ISRFactory.h"
#include "IdentityISR.h"
#include "TextPreprocessor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace mithril; // TODO: bad, put everything in a namespace{} instead

// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5;


// namespace mithril {

namespace mithril {
namespace detail {

inline mithril::FieldType TokenTypeToField(TokenType token_type) {
    switch (token_type) {
        case TokenType::WORD:
            return mithril::FieldType::ALL;
        case TokenType::TITLE:
            return mithril::FieldType::TITLE;
        case TokenType::URL:
            return mithril::FieldType::URL;
        case TokenType::ANCHOR:
            return mithril::FieldType::ANCHOR;
        case TokenType::DESC:
            return mithril::FieldType::DESC;
        case TokenType::BODY:
            return mithril::FieldType::BODY;
        default: // WARNING: this should not happen
            return mithril::FieldType::ALL;
    }
}

}  // detail
}  // mithril

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

};

class TermQuery : public Query {
public:
    TermQuery(Token token, const core::MemMapFile& index_file,
              mithril::TermDictionary& term_dict, mithril::PositionIndex& position_index)
        : token_(std::move(token)), index_file_(index_file),
          term_dict_(term_dict), position_index_(position_index) {}

    Token get_token() { return token_; }

    std::vector<uint32_t> evaluate() const override {
        TermReaderFactory term_reader_factory(index_file_, term_dict_, position_index_);
        const auto field = mithril::detail::TokenTypeToField(token_.type);
        auto term = term_reader_factory.CreateISR(token_.value, field);

        std::vector<uint32_t> results;
        results.reserve(MAX_DOCUMENTS);

        while (term->hasNext()) {
            const uint32_t docId = term->currentDocID();
            results.emplace_back(docId);
            term->moveNext();
        }

        return results;
    }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {
        TermReaderFactory term_reader_factory(index_file_, term_dict_, position_index_);
        const auto field = mithril::detail::TokenTypeToField(token_.type);
        return term_reader_factory.CreateISR(token_.value, field);
    }

    [[nodiscard]] std::string to_string() const override { 
        return "TERM(" + token_.value + " " + token_.toString() + ")";
         }

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
        auto left_isr = left_->generate_isr();
        auto right_isr = right_->generate_isr();

        // TODO: decide if this is the right behaviour
        if (left_isr->isIdentity() && right_isr->isIdentity()) {
            return std::make_unique<mithril::IdentityISR>();
        } else if (left_isr->isIdentity()) {
            return std::move(right_isr);
        } else if (right_isr->isIdentity()) {
            return std::move(left_isr);
        } else {
            std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
            readers.push_back(std::move(left_isr));
            readers.push_back(std::move(right_isr));
            return std::make_unique<mithril::TermAND>(std::move(readers));
        }
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
        auto left_isr = left_->generate_isr();
        auto right_isr = right_->generate_isr();

        // TODO: decide if this is the right behaviour
        if (left_isr->isIdentity() && right_isr->isIdentity()) {
            return std::make_unique<mithril::IdentityISR>();
        } else if (left_isr->isIdentity()) {
            return std::move(right_isr);
        } else if (right_isr->isIdentity()) {
            return std::move(left_isr);
        } else {
            std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
            readers.push_back(std::move(left_isr));
            readers.push_back(std::move(right_isr));
            return std::make_unique<mithril::TermOR>(std::move(readers));
        }
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


class QuoteQuery : public Query {
public:
    QuoteQuery(Token quote_token,
               const core::MemMapFile& index_file,
               mithril::TermDictionary& term_dict,
               mithril::PositionIndex& position_index)
        : quote_token_(std::move(quote_token)),
          index_file_(index_file),
          term_dict_(term_dict),
          position_index_(position_index) {}

    std::vector<uint32_t> evaluate() const override { return {}; }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {

        std::vector<std::string> quote_terms = ExtractQuoteTerms(quote_token_);
        std::cout << "Quote terms: ";
        for (const auto& term : quote_terms) {
            std::cout << "Quote term: " << term << std::endl;
        }

        return std::make_unique<::mithril::TermQuote>(
            query::QueryConfig::GetIndexPath(),
            quote_terms,
            index_file_,
            term_dict_,
            position_index_
        );
    }

    [[nodiscard]] std::string to_string() const override { return "QUOTE(" + quote_token_.value + ")"; }

    [[nodiscard]] std::string get_type() const override { return "QuoteQuery"; }

private:
    Token quote_token_;
    const core::MemMapFile& index_file_;
    mithril::TermDictionary& term_dict_;
    mithril::PositionIndex& position_index_;
};

class PhraseQuery : public Query {
public:
    PhraseQuery(Token phrase_token,
               const core::MemMapFile& index_file,
               mithril::TermDictionary& term_dict,
               mithril::PositionIndex& position_index)
        : phrase_token_(std::move(phrase_token)),
          index_file_(index_file),
          term_dict_(term_dict),
          position_index_(position_index) {}

    std::vector<uint32_t> evaluate() const override { return {}; }

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override {
        std::vector<std::string> phrase_terms = ExtractQuoteTerms(phrase_token_);
        std::cout << "Phrase terms: ";
        for (const auto& term : phrase_terms) {
            std::cout << "Phrase term: " << term << std::endl;
        }

        // Use TermPhrase for fuzzy phrase matching
        return std::make_unique<::mithril::TermPhrase>(
            query::QueryConfig::GetIndexPath(),
            phrase_terms,
            index_file_,
            term_dict_,
            position_index_
        );
    }

    [[nodiscard]] std::string to_string() const override { return "PHRASE(" + phrase_token_.value + ")"; }

    [[nodiscard]] std::string get_type() const override { return "PhraseQuery"; }

private:
    Token phrase_token_;
    const core::MemMapFile& index_file_;
    mithril::TermDictionary& term_dict_;
    mithril::PositionIndex& position_index_;
};

#endif  // QUERY_H_
