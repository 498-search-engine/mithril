#include "BM25.h"
#include "PositionIndex.h"
#include "TermDictionary.h"
#include "data/Document.h"

#include <regex>
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
                       const std::vector<int>& stopwordIdx,
                       const std::vector<int>& nonstopwordIdx,
                       const data::Document& doc,
                       const data::DocInfo& info,
                       const PositionIndex& position_index,
                       const std::unordered_map<std::string, uint32_t>& termFreq,
                       std::unordered_map<std::string, const char*>& data);

std::vector<std::pair<std::string, int>>
TokenifyQuery(const std::string& query, std::vector<int>& stopwordIdx, std::vector<int>& nonstopwordIdx);

inline bool ContainsPornKeywords(const std::string& input) {
    // Precompiled regex pattern (optimized for performance)
    static const std::regex pornPattern(R"((?:p[0o]rn|\bs[e3]x\b|xxx|nsfw|nudes?|fetish|blow[-_]?job))",
                                        std::regex_constants::icase | std::regex_constants::optimize);
    return std::regex_search(input, pornPattern);
}

inline bool ContainsPornKeywords(const std::vector<std::string>& input) {
    for (const auto& in : input) {
        if (ContainsPornKeywords(in)) {
            return true;
        }
    }
    return false;
}
}  // namespace mithril::ranking
