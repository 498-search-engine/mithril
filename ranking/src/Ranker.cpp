#include "Ranker.h"

#include "DynamicRanker.h"
#include "StaticRanker.h"
#include "TextPreprocessor.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <spdlog/spdlog.h>

#define LOGGING 1

namespace mithril::ranking {

uint32_t GetFinalScore(const std::vector<std::pair<std::string, int>>& query,
                       const data::Document& doc,
                       const data::DocInfo& info,
                       const PositionIndex& position_index) {

    auto logger = spdlog::get("ranker_logger");
    if (!logger) {
        logger = spdlog::basic_logger_mt("ranker_logger", "ranker.log");
    }

    std::string title;
    for (const auto& term : doc.title) {
        title += term;
    }
    std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return std::tolower(c); });

    bool isInURL = true;
    bool isInTitle = true;
    bool isInDescription = true;
    bool isInBody = true;

#if LOGGING == 1
    logger->info("[{}] Query: {}, URL: {}, Title: {}", doc.id, query[0].first, doc.url, title);
#endif

    float totalTermsSize = (float)query.size();

    // Percentage feature (i.e whether each tokenized word exists in this space or not)
    float wordsInUrl = 0;
    float wordsInTitle = 0;
    float wordsInDesc = 0;
    float wordsInBody = 0;

    for (const auto& [term, multiplicity] : query) {
        std::vector<uint16_t> urlPositions =
            position_index.getPositions(mithril::TokenNormalizer::decorateToken(term, FieldType::URL), doc.id);
        std::vector<uint16_t> titlePositions =
            position_index.getPositions(mithril::TokenNormalizer::decorateToken(term, FieldType::TITLE), doc.id);
        std::vector<uint16_t> descPositions =
            position_index.getPositions(mithril::TokenNormalizer::decorateToken(term, FieldType::DESC), doc.id);
        std::vector<uint16_t> bodyPositions =
            position_index.getPositions(mithril::TokenNormalizer::decorateToken(term, FieldType::BODY), doc.id);

        bool termInUrl = urlPositions.size();
        bool termInTitle = titlePositions.size();
        bool termInDescription = descPositions.size();
        bool termInBody = bodyPositions.size();

        termInUrl = doc.url.find(term) != std::string::npos;
        termInTitle = title.find(term) != std::string::npos;

        if (!termInUrl) {
            isInURL = false;
        } else {
            wordsInUrl++;
        }

        if (!termInTitle) {
            isInTitle = false;
        } else {
            wordsInTitle++;
        }

        if (!termInDescription) {
            isInDescription = false;
        } else {
            wordsInDesc++;
        }

        if (!termInBody) {
            isInBody = false;
        } else {
            wordsInBody++;
        }
    }

    dynamic::RankerFeatures features{
        // Boolean presence flags
        .query_in_url = isInURL,
        .query_in_title = isInTitle,
        .query_in_description = isInDescription,
        .query_in_body = isInBody,

        // Query Coverage percentage features
        .coverage_percent_query_url = (wordsInUrl / totalTermsSize),
        .coverage_percent_query_title = (wordsInTitle / totalTermsSize),
        .coverage_percent_query_description = (wordsInDesc / totalTermsSize),

        // Query Density percentage features

        // Position features

        // Precomputed scores
        .static_rank = static_cast<float>(GetUrlStaticRank(doc.url)),
        .pagerank = info.pagerank_score,
    };

    return ranking::dynamic::GetUrlDynamicRank(features);
}
}  // namespace mithril::ranking
