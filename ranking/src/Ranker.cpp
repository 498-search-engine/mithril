#include "Ranker.h"

#include "DynamicRanker.h"
#include "StaticRanker.h"

namespace mithril::ranking {
uint32_t GetFinalScore(std::vector<std::string> query, const data::Document& doc, const data::DocInfo& info) {
    dynamic::RankerFeatures features{
        .static_rank = static_cast<float>(GetUrlStaticRank(doc.url)),
        .pagerank = info.pagerank_score,
    };

    return ranking::dynamic::GetUrlDynamicRank(features);
}
}  // namespace mithril::ranking
