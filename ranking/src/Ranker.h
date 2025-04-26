#include "BM25.h"
#include "PositionIndex.h"
#include "TermDictionary.h"
#include "data/Document.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking {

namespace {
bool StartsWithStrict(const std::string& token, const std::string& word) {
    return token.starts_with(word) && token != word;
}
}  // namespace
inline bool IsValidToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    if (token == "AND" || token == "OR" || token == "NOT") {
        return false;
    }

    if (StartsWithStrict(token, "title") || StartsWithStrict(token, "url") || StartsWithStrict(token, "anchor") ||
        StartsWithStrict(token, "desc")) {
        return false;
    }

    return true;
}

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

std::vector<std::pair<std::string, int>> TokenifyQuery(const std::string& query);

}  // namespace mithril::ranking
