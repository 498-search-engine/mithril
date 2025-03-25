// Define a grammar in here

#ifndef QUERY_H_
#define QUERY_H_

#include <_types/_uint32_t.h>
#include <vector>
#include <utility>
#include <string>
#include "Token.h"
#include "../../index/src/TermReader.h"
#include "QueryConfig.h"
#include "intersect.h"
#include <unordered_map>


// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5; 

class Query {
public:
    virtual ~Query() {}
    [[nodiscard]] virtual std::vector<uint32_t> Evaluate() const { return {}; }

    // [[nodiscard]] virtual mithril::IndexStreamReader GetISR() const { return {}; }

};

class TermQuery : public Query {
public:
    TermQuery(Token token) : token_(std::move(token)) {}

    Token getToken() { return token_; }

    std::vector<uint32_t> Evaluate() const override {
        mithril::TermReader term(query::QueryConfig::IndexPath, token_.value);
        // mithril::DocumentMapReader doc_reader(query::QueryConfig::IndexPath);

        std::vector<uint32_t> results;
        results.reserve(MAX_DOCUMENTS);

        while (term.hasNext()) {
            const uint32_t docId = term.currentDocID();
            results.emplace_back(docId);
            term.moveNext();
        }

        return results;
    }

private:
    Token token_;
    static constexpr int MAX_DOCUMENTS = 100000;
};



class AndQuerySimd : public Query {
    // Combines results where ALL terms must match
    Query* left_; 
    Query* right_; 

public: 
    AndQuerySimd(Query* left, Query* right) : left_(left), right_(right) {
        if (!left && !right){
            std::cerr << "Need a left and right query\n";
            exit(1);
        }
    }

    std::vector<uint32_t> Evaluate() const override {
        std::vector<uint32_t> left_docs = left_->Evaluate();
        std::vector<uint32_t> right_docs = right_->Evaluate();
        
        //   auto intersected_ids = intersect_gallop_vec(left_docs, right_docs);
        // return intersected_ids;
        // Create a vector with enough space for results
        std::vector<uint32_t> results(std::min(left_docs.size(), right_docs.size()));
        
        // Call SIMD intersection
        size_t result_size = intersect_simd_sse(
            left_docs.data(), right_docs.data(), 
            results.data(), left_docs.size(), right_docs.size());
        
        // Resize to actual result size
        results.resize(result_size);
        return results;
    }
};

// ... existing code ...

class AndQuery : public Query {
    // Combines results where ALL terms must match
    Query* left_; 
    Query* right_; 

public: 
    AndQuery(Query* left, Query* right) : left_(left), right_(right) {
        if (!left && !right){
            std::cerr << "Need a left and right query\n";
            exit(1);
        }
    }

    std::vector<uint32_t> Evaluate() const override {
        // Get the left and right ISRs
        mithril::TermReader left_isr = getISR(left_);
        mithril::TermReader right_isr = getISR(right_);
        
        std::vector<uint32_t> results;
        results.reserve(MAX_DOCUMENTS);
        
        // Continue while both iterators have documents
        while (left_isr.hasNext() && right_isr.hasNext()) {
            uint32_t left_doc_id = left_isr.currentDocID();
            uint32_t right_doc_id = right_isr.currentDocID();
            
            if (left_doc_id == right_doc_id) {
                // Match found, add to results
                results.push_back(left_doc_id);
                // Advance both readers
                left_isr.moveNext();
                right_isr.moveNext();
            } else if (left_doc_id < right_doc_id) {
                // Left is behind, advance it
                left_isr.moveNext();
            } else {
                // Right is behind, advance it
                right_isr.moveNext();
            }
            
            // Stop if we've collected enough documents
            if (results.size() >= MAX_DOCUMENTS) {
                break;
            }
        }
        
        return results;
    }

private:
    // Helper method to get an ISR from a query
    mithril::TermReader getISR(Query* query) const {
        // This is a simplification - in a real implementation,
        // you'd want to add methods to get the proper ISRs from each query type
        // For now, we're assuming all queries can be converted to a term reader
        if (auto* term_query = dynamic_cast<TermQuery*>(query)) {
            return mithril::TermReader(query::QueryConfig::IndexPath, 
                                     term_query->getToken().value);
        }
        
        // Handle error case
        std::cerr << "Unable to convert query to ISR\n";
        return mithril::TermReader(query::QueryConfig::IndexPath, "");
    }
};



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

