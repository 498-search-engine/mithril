// Define a grammar in here

#ifndef QUERY_H_
#define QUERY_H_

#include <vector>
#include <utility>
#include <string>
#include "Token.h"
#include "../../index/src/TermReader.h"
#include "../../index/src/DocumentMapReader.h"
#include "QueryConfig.h"

struct DocInfo {
    uint32_t doc_id;
    uint32_t frequency;
    std::string url;
    std::vector<std::string> title;
    std::vector<uint32_t> positions;

    DocInfo(uint32_t id, uint32_t freq) : doc_id(id), frequency(freq) {}
};


// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5; 

class Query {
public:
    virtual ~Query() {}
    [[nodiscard]] virtual std::vector<DocInfo> Evaluate() const { return {}; }
};

class TermQuery : public Query {
public:
    TermQuery(Token token) : token_(std::move(token)) {}

    std::vector<DocInfo> Evaluate() const override {
        mithril::TermReader term(query::QueryConfig::IndexPath, token_.value);
        mithril::DocumentMapReader doc_reader(query::QueryConfig::IndexPath);

        std::vector<DocInfo> results;
        results.reserve(MAX_DOCUMENTS);

        while (term.hasNext()) {
            uint32_t doc_id = term.currentDocID();
            uint32_t freq = term.currentFrequency();
            
            DocInfo doc(doc_id, freq);
            
            // Get document metadata
            auto doc_opt = doc_reader.getDocument(doc_id);
            if (doc_opt) {
                doc.url = doc_opt->url;
                doc.title = doc_opt->title;
            }
            
            // Get positions if available
            if (term.hasPositions()) {
                doc.positions = term.currentPositions();
            }
            
            results.push_back(std::move(doc));
            term.moveNext();
        }

        return results;
    }

private:
    Token token_;
    static constexpr int MAX_DOCUMENTS = 100000;
};

#endif // QUERY_H_

