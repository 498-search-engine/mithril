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

    uint8_t fieldFlags = position_index.getFieldFlags(query[0].first, doc.id);
    bool isInURL = fieldTypeToFlag(FieldType::URL) & fieldFlags;
    bool isInTitle = fieldTypeToFlag(FieldType::TITLE) & fieldFlags;
    bool isInDescription = fieldTypeToFlag(FieldType::DESC) & fieldFlags;
    bool isInBody = fieldTypeToFlag(FieldType::BODY) & fieldFlags;

    std::string title;

    for (const auto& term : doc.title) {
        title += term;
    }

    std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return std::tolower(c); });

    for (const auto& [term, multiplicity] : query) {
        if (doc.url.find(term) != std::string::npos) {
            if (!isInURL) {
                logger->error("Term {} not found in URL", term);
            }
            isInURL = true;
        }
        if (title.find(term) != std::string::npos) {
            if (!isInURL) {
                logger->error("Term {} not found in title", term);
            }
            isInTitle = true;
        }
    }

    logger->info("\nQuery: {}, URL: {}, Title: {}", query[0].first, doc.url, title);

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
