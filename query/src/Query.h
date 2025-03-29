// Define a grammar in here

#ifndef QUERY_H_
#define QUERY_H_

#include <_types/_uint32_t.h>
#include <memory>
#include <vector>
#include <utility>
#include <string>
#include "Token.h"
#include "../../index/src/TermReader.h"
#include "../../index/src/TermAND.h"
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
        return std::make_unique<mithril::TermAND>(std::move(readers));
    }
};




// class AndQuerySimd : public Query {
//     // Combines results where ALL terms must match
//     Query* left_; 
//     Query* right_; 

// public: 
//     AndQuerySimd(Query* left, Query* right) : left_(left), right_(right) {
//         if (!left && !right){
//             std::cerr << "Need a left and right query\n";
//             exit(1);
//         }
//     }

//     std::vector<uint32_t> Evaluate() const override {
//         std::vector<uint32_t> left_docs = left_->Evaluate();
//         std::vector<uint32_t> right_docs = right_->Evaluate();
        
//         //   auto intersected_ids = intersect_gallop_vec(left_docs, right_docs);
//         // return intersected_ids;
//         // Create a vector with enough space for results
//         std::vector<uint32_t> results(std::min(left_docs.size(), right_docs.size()));
        
//         // Call SIMD intersection
//         size_t result_size = intersect_simd_sse(
//             left_docs.data(), right_docs.data(), 
//             results.data(), left_docs.size(), right_docs.size());
        
//         // Resize to actual result size
//         results.resize(result_size);
//         return results;
//     }
// };



// class OrQuery : public Query {
//     // Combines results where ALL terms must match
//     Query* left_; 
//     Query* right_; 

// public: 
//     OrQuery(Query* left, Query* right) : left_(left), right_(right) {
//         if (!left && !right){
//             std::cerr << "Need a left and right query\n";
//             exit(1);
//         }
//     }

//     std::vector<uint32_t> Evaluate() const override {
//         std::vector<uint32_t> left_docs = left_->Evaluate();
//         std::vector<uint32_t> right_docs = right_->Evaluate();
//         auto intersected_ids = (left_docs, right_docs);
//         return intersected_ids;
//     }
// };


// class OrQuery : public Query {
//     // Combines results where ANY terms can match
// };

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

