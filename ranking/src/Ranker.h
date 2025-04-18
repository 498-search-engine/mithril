#include "data/Document.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking {
/**
 * query is {term, multiplicity}
 * e.g A and (B or A) => {“A”: 2”, “B”: 1}
 */
uint32_t GetFinalScore(const std::vector<std::pair<std::string, int>>& query,
                       const data::Document& doc,
                       const data::DocInfo& info);
}  // namespace mithril::ranking
