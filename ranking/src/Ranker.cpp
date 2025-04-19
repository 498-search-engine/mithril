#include "Ranker.h"

#include "DynamicRanker.h"
#include "StaticRanker.h"
#include "TextPreprocessor.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking {

uint32_t GetFinalScore(const std::vector<std::pair<std::string, int>>& query,
                       const data::Document& doc,
                       const data::DocInfo& info,
                       const PositionIndex& position_index) {

    auto logger = spdlog::get("ranker_logger");
    if (!logger) {
        logger = spdlog::basic_logger_mt("ranker_logger", "ranker.log");
    }

    std::vector<uint16_t> urlPositions =
        position_index.getPositions(mithril::TokenNormalizer::decorateToken(query[0].first, FieldType::URL), doc.id);
    std::vector<uint16_t> titlePositions =
        position_index.getPositions(mithril::TokenNormalizer::decorateToken(query[0].first, FieldType::TITLE), doc.id);
    std::vector<uint16_t> descPositions =
        position_index.getPositions(mithril::TokenNormalizer::decorateToken(query[0].first, FieldType::DESC), doc.id);
    std::vector<uint16_t> bodyPositions =
        position_index.getPositions(mithril::TokenNormalizer::decorateToken(query[0].first, FieldType::BODY), doc.id);

    bool isInURL = urlPositions.size();
    bool isInTitle = titlePositions.size();
    bool isInDescription = descPositions.size();
    bool isInBody = bodyPositions.size();

    std::string title;
    for (const auto& term : doc.title) {
        title += term;
    }
    std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return std::tolower(c); });

    logger->info("[{}] Query: {}, URL: {}, Title: {}", doc.id, query[0].first, doc.url, title);

    // for (const auto& [term, multiplicity] : query) {
    //     if (doc.url.find(term) != std::string::npos) {
    //         if (!isInURL) {
    //             logger->error("Term {} not found in URL", term);
    //         }
    //         isInURL = true;
    //     }
    //     if (title.find(term) != std::string::npos) {
    //         if (!isInURL) {
    //             logger->error("Term {} not found in title", term);
    //         }
    //         isInTitle = true;
    //     }
    // }

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
