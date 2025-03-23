
// Define a grammar in here

#ifndef QUERY_H_
#define QUERY_H_


#include <Token.h>
#include <_types/_uint32_t.h>
#include <iostream>
#include <utility>
#include <vector>
#include "../../index/src/TermReader.h"
#include "QueryConfig.h"

// TODO: Determine if we should limit the number of documents we read
using DocIDArray = std::vector<int32_t>;
const int MAX_DOCUMENTS = 1e5; 

class Query {
public:

    virtual ~Query(){}
    [[nodiscard]] virtual auto Evaluate() const -> std::vector<uint32_t>{return {}; } 
};

class TermQuery : protected Query {
public:
    TermQuery(Token token) : token_(std::move(token)) {}

    auto Evaluate() -> std::vector<uint32_t> const {

        mithril::TermReader term(query::QueryConfig::IndexPath, token_.value);

        // TODO: Determine optimal size of vector
        std::vector<uint32_t> data;
        data.reserve(MAX_DOCUMENTS);

        while (term.hasNext()){
            data.emplace_back(term.currentDocID());
            term.moveNext();
        }

        return data; 
    }

private:
    Token token_;
};

#endif // QUERY_H_

