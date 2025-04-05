#ifndef PAGERANK_H
#define PAGERANK_H

#include "core/config.h"
#include "core/csr.h"
#include "data/Document.h"

#include <vector>

constexpr double ErrorAllowed = 0.001;

namespace mithril::pagerank {
/**
    @brief Performs page rank on a CSR Matrix and number of nodes and stores it in Results.
*/
void PerformPageRank(core::CSRMatrix& matrix_, int N);

/**
    @brief Builds the CSR matrix and performs page rank on it.
*/
void PerformPageRank();
void PerformDomainRank();

/**
    variables that external users might care about
*/
extern std::vector<double> Results;
extern std::unordered_map<data::docid_t, data::Document> NodeToDocument;

/**
    private namespace variables
*/
static core::Config Config = core::Config("pagerank.conf");
const static inline std::string InputDirectory =
    std::string(Config.GetString("document_folder").Cstr());
static int Nodes = 0;
};  // namespace mithril::pagerank
#endif