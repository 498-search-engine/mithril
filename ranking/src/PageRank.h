#ifndef PAGERANK_H
#define PAGERANK_H

static_assert(sizeof(double) == 8, "Size of double is not 8 bytes");

#include "core/config.h"
#include "core/csr.h"
#include "core/memory.h"
#include "data/Document.h"

#include <vector>

constexpr double ErrorAllowed = 0.001;

namespace mithril::pagerank {
/**
    @brief Performs page rank on a CSR Matrix and number of nodes and stores it in Results.
*/
void PerformPageRank(core::CSRMatrix& matrix_, int N);

/**
    @brief Gets the node ID for a particular link. If it doesn't exist, it ssigns it one.
*/
int GetLinkNode(const std::string& link);

/**
    @brief Writes PageRank info per docID to a file in byte format (big endian)
*/
void Write();

/**
    @brief Builds the CSR matrix and performs page rank on it.
*/
void PerformPageRank();

/**
    @brief Cleanup all associated memory with PageRank.
*/
void Cleanup();

/**
    variables that external users might care about
    unique pointers so memory can be completely freed once this is no longer needed
*/
extern core::UniquePtr<std::unordered_map<std::string, int>> LinkToNode;
extern core::UniquePtr<std::unordered_map<int, std::string>> NodeToLink;
extern core::UniquePtr<std::unordered_map<int, std::vector<int>>> NodeConnections;
extern core::UniquePtr<std::unordered_map<data::docid_t, data::Document>> NodeToDocument;
extern core::UniquePtr<std::vector<double>> Results;

extern int Nodes;
/**
    private namespace variables
*/
static core::Config Config = core::Config("pagerank.conf");
const static inline std::string InputDirectory = std::string(Config.GetString("document_folder").Cstr());
const static inline std::string OutputFile = std::string(Config.GetString("output_file").Cstr());

};  // namespace mithril::pagerank
#endif