#include "BM25.h"
#include "PositionIndex.h"
#include "TermDictionary.h"
#include "data/Document.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking {
/**
 * query is {term, multiplicity}
 * e.g A and (B or A) => {“A”: 2”, “B”: 1}
 */

std::unordered_map<std::string, uint32_t> GetDocumentFrequencies(const TermDictionary& term_dict,
                                                                 const std::vector<std::pair<std::string, int>>& query);

uint32_t GetFinalScore(BM25* BM25Lib,
                       const std::vector<std::pair<std::string, int>>& query,
                       const data::Document& doc,
                       const data::DocInfo& info,
                       const PositionIndex& position_index,
                       const std::unordered_map<std::string, uint32_t>& termFreq,
                       std::unordered_map<std::string, const char*>& data);

}  // namespace mithril::ranking
