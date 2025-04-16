#ifndef PAGERANK_H
#define PAGERANK_H

static_assert(sizeof(float) == 4, "Size of float is not 4 bytes");
static_assert(sizeof(double) == 8, "Size of double is not 8 bytes");

#include "core/config.h"
#include "core/csr.h"
#include "core/memory.h"
#include "data/Document.h"

#include <vector>

constexpr float ErrorAllowed = 0.001;

namespace mithril::pagerank {

struct PagerankDocument {
    mithril::data::docid_t id;
    std::string url;
};

/**
    @brief Process any links as necessary (e.g for DomainRank.)
*/
std::string ProcessLink(const std::string& link);

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
    @brief Cleanup all associated memory with PageRank.
*/
void Cleanup();

/**
    variables that external users might care about
    unique pointers so memory can be completely freed once this is no longer needed
*/
extern core::UniquePtr<std::unordered_map<std::string, int>> LinkToNode;
extern core::UniquePtr<std::vector<std::vector<int>>> NodeConnections;
extern core::UniquePtr<std::unordered_map<int, PagerankDocument>> NodeToDocument;
extern core::UniquePtr<std::unordered_map<data::docid_t, int>> DocumentToNode;
extern core::UniquePtr<std::vector<float>> Results;
extern core::UniquePtr<std::vector<float>> StandardizedResults;

extern int Nodes;
extern size_t DocumentCount;
/**
    private namespace variables
*/
static core::Config Config = core::Config("pagerank.conf");
const static inline std::string InputDirectory = std::string(Config.GetString("document_folder").Cstr());
const static inline std::string OutputFile = std::string(Config.GetString("output_file").Cstr());

/**
    @brief Builds the CSR matrix and performs page rank on it.
*/
void PerformPageRank(const std::string& inputDirectory = InputDirectory);

};  // namespace mithril::pagerank
#endif