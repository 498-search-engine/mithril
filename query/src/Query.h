// Define a grammar in here

#ifndef QUERY_H_
#define QUERY_H_

#include <_types/_uint32_t.h>
#include <memory>
#include <vector>
#include <utility>
#include <string>
#include "TermOR.h"
#include "Token.h"
#include "../../index/src/TermReader.h"
#include "../../index/src/TermAND.h"
#include "../../index/src/TermOR.h"
#include "QueryConfig.h"
#include "intersect.h"
#include <unordered_map>


// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5; 

class Query {
public:
    virtual ~Query() {}

    // Evaluates everything in one go 
    [[nodiscard]] virtual std::vector<uint32_t> evaluate() const { return {}; }

    // Helps us do some more fine grained stream reading
    virtual uint32_t get_next_doc() const { return 0; }
    virtual bool has_next() const { return false; }
    
    [[nodiscard]] virtual std::unique_ptr<mithril::IndexStreamReader> generate_isr() const {
        return nullptr; 
    };

private: 
};

class TermQuery : public Query {
public:
    TermQuery(Token token) : token_(std::move(token)) {
    }

    Token get_token() { return token_; }

    std::vector<uint32_t> evaluate() const override {
        mithril::TermReader term(query::QueryConfig::IndexPath, token_.value);

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
        return std::make_unique<mithril::TermReader>(query::QueryConfig::IndexPath, token_.value);
    }

private:
    Token token_;
    static constexpr int MAX_DOCUMENTS = 100000;
};


class AndQuery :  public Query {
    Query* left_; 
    Query* right_; 

public: 

    AndQuery(Query* left, Query* right) : left_(left), right_(right) {
        if (!left && !right){
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
};

class OrQuery :  public Query {
    Query* left_; 
    Query* right_; 

public: 

    OrQuery(Query* left, Query* right) : left_(left), right_(right) {
        if (!left && !right){
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
};


// class NotQuery : public Query {
//     // Excludes documents matching the negated term
// };

// class FieldQuery : public Query {
//     // Restricts search to specific fields (TITLE, TEXT)
// };

// class PhraseQuery : public Query {
//     // For exact phrase matching with quotes
//     // Will need to use position information
// };

#endif // QUERY_H_

