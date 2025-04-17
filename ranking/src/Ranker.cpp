#include "Ranker.h"

#include "DynamicRanker.h"
#include "StaticRanker.h"
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

    bool isInURL = false;
    bool isInTitle = false;
    bool isInDescription = false;
    bool isInBody = false;

    std::string title;
    std::string description;
    std::string body;

    for (const auto& term : doc.title) {
        std::cout << "document has title: " << term << std::endl;
        title += term;
    }

    for (const auto& term : doc.description) {
        std::cout << "document has descript: " << term << std::endl;
        description += term;
    }

    for (const auto& term : doc.words) {
        std::cout << "document has word: " << term << std::endl;
        body += term;
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

    logger->info("\nQuery: {}, URL: {}, Title: {}", query[0].first, doc.url, title);
    logger->info("Description: {}, Body: {}", description, body);

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
