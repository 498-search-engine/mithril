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
#include "core/pair.h"
#include <vector>
#include <sstream>

using namespace mithril; // TODO: bad, put everything in a namespace{} instead

// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5;


// namespace mithril {

namespace mithril {
namespace detail {

inline mithril::FieldType TokenTypeToField(TokenType token_type);

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

    std::vector<uint32_t> evaluate() const override;

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override;

    [[nodiscard]] std::string to_string() const override;

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

    std::vector<uint32_t> evaluate() const override;

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override;

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::string get_type() const override { return "AndQuery"; }
};

class OrQuery : public Query {
    Query* left_;
    Query* right_;

public:
    OrQuery(Query* left, Query* right);

    std::vector<uint32_t> evaluate() const override;

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override;

    [[nodiscard]] std::string to_string() const override {
        return "OR(" + left_->to_string() + ", " + right_->to_string() + ")";
    }

    [[nodiscard]] std::string get_type() const override { return "OrQuery"; }
};


class NotQuery : public Query {
public:
    NotQuery(Query* expression);

    std::vector<uint32_t> evaluate() const override;

    [[nodiscard]] std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override;

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

    std::vector<uint32_t> evaluate() const override;

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override;

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

    std::vector<uint32_t> evaluate() const override;

    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const override;

    [[nodiscard]] std::string to_string() const override { return "PHRASE(" + phrase_token_.value + ")"; }

    [[nodiscard]] std::string get_type() const override { return "PhraseQuery"; }

private:
    Token phrase_token_;
    const core::MemMapFile& index_file_;
    mithril::TermDictionary& term_dict_;
    mithril::PositionIndex& position_index_;
};

#endif  // QUERY_H_
