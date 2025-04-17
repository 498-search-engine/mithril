#include "Ranker.h"

#include "DynamicRanker.h"
#include "StaticRanker.h"

namespace mithril::ranking {

uint32_t GetFinalScore(const std::vector<std::pair<std::string, int>>& query,
                       const data::Document& doc,
                       const data::DocInfo& info) {

    bool isInURL = false;
    bool isInTitle = false;
    bool isInDescription = false;
    bool isInBody = false;

    std::string title;
    std::string description;
    std::string body;

    for (const auto& term : doc.title) {
        title += term;
    }

    for (const auto& term : doc.description) {
        title += term;
    }

    for (const auto& term : doc.words) {
        title += term;
    }

    for (const auto& [term, multiplicity] : query) {
        if (doc.url.find(term) != std::string::npos) {
            isInURL = true;
        }
        if (title.find(term) != std::string::npos) {
            isInTitle = true;
        }
        if (description.find(term) != std::string::npos) {
            isInDescription = true;
        }
        if (body.find(term) != std::string::npos) {
            isInBody = true;
        }
    }

    dynamic::RankerFeatures features{
        // Boolean presence flags
        .query_in_url = isInURL,
        .query_in_title = isInTitle,
        .query_in_description = isInDescription,
        .query_in_body = isInBody,
        .query_in_order = false,  // TODO: implement this

        // Precomputed scores
        .static_rank = static_cast<float>(GetUrlStaticRank(doc.url)),
        .pagerank = info.pagerank_score,
    };

    return ranking::dynamic::GetUrlDynamicRank(features);
}
}  // namespace mithril::ranking
