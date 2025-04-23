#include "Ranker.h"

#include "BM25.h"
#include "DynamicRanker.h"
#include "PositionIndex.h"
#include "StaticRanker.h"
#include "TermDictionary.h"
#include "TextPreprocessor.h"
#include "data/Document.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

#define LOGGING 0

namespace mithril::ranking {

namespace {
int CountWordOccurrences(const std::string& text, const std::string& word) {
    int count = 0;
    std::string tempText = text;
    std::string tempWord = word;

    std::transform(tempText.begin(), tempText.end(), tempText.begin(), ::tolower);
    std::transform(tempWord.begin(), tempWord.end(), tempWord.begin(), ::tolower);

    size_t position = tempText.find(tempWord, 0);
    while (position != std::string::npos) {
        count++;
        position = tempText.find(tempWord, position + tempWord.length());
    }
    return count;
}
}  // namespace

std::unordered_map<std::string, uint32_t>
GetDocumentFrequencies(const TermDictionary& term_dict, const std::vector<std::pair<std::string, int>>& query) {
    std::unordered_map<std::string, uint32_t> map;
    for (const auto& [query, multiplicity] : query) {
        auto it = map.find(query);
        if (it != map.end()) {
            continue;
        }


        std::optional<TermDictionary::TermEntry> termEntry = term_dict.lookup(query);
        if (!termEntry.has_value()) {
            map[query] = 0;
        } else {
            map[query] = termEntry->postings_count;
        }
    }

    return map;
}

uint32_t GetFinalScore(const std::vector<std::pair<std::string, int>>& query,
                       const data::Document& doc,
                       const data::DocInfo& info,
                       const PositionIndex& position_index,
                       const std::unordered_map<std::string, uint32_t>& termFreq) {

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

    float earliestPosTitle = 0.0F;
    float earliestPosBody = 0.0F;

    float densityUrl = 0.0F;
    float densityTitle = 0.0F;
    float densityDescription = 0.0F;

    float weightedBM25 = 0.0F;

    for (const auto& [term, multiplicity] : query) {
        std::vector<uint16_t> bodyPositions = position_index.getPositions(term, doc.id);

        bool termInDescription =
            position_index.hasPositions(mithril::TokenNormalizer::decorateToken(term, FieldType::DESC), doc.id);
        bool termInBody = bodyPositions.size() > 0;
        bool termInUrl = doc.url.find(term) != std::string::npos;

        size_t pos = title.find(term);
        bool termInTitle = pos != std::string::npos;

        if (!termInUrl) {
            isInURL = false;
        } else {
            wordsInUrl++;

            size_t urlOccurences = std::min(CountWordOccurrences(doc.url, term) * term.size(), doc.url.size());
            densityUrl += (static_cast<float>(urlOccurences) / static_cast<float>(doc.url.size())) *
                          (static_cast<float>(multiplicity) / static_cast<float>(query.size()));
        }

        if (!termInTitle) {
            isInTitle = false;
        } else {
            wordsInTitle++;
            earliestPosTitle += (1 / static_cast<float>(pos + 1)) *
                                (static_cast<float>(multiplicity) / static_cast<float>(query.size()));

            int titleOccurences = std::min(CountWordOccurrences(title, term), (int)doc.title.size());
            densityTitle += (static_cast<float>(titleOccurences) / static_cast<float>(doc.title.size())) *
                            (static_cast<float>(multiplicity) / static_cast<float>(query.size()));
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
            earliestPosBody += (1 / static_cast<float>(bodyPositions[0] + 1)) *
                               (static_cast<float>(multiplicity) / static_cast<float>(query.size()));
        }

        weightedBM25 += static_cast<float>(BM25Lib.ScoreTermForDoc(info, termFreq.at(term), bodyPositions.size())) *
                        (static_cast<float>(multiplicity) / static_cast<float>(query.size()));
    }

    float orderedTitleScore = std::sqrt(ranking::dynamic::OrderedMatchScore(query, doc.title));

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

        .order_sensitive_title = orderedTitleScore,
        // Query Density percentage features
        .density_percent_query_url = densityUrl,
        .density_percent_query_title = densityTitle,
        .density_percent_query_description = densityDescription,

        // Position features
        .earliest_pos_title = earliestPosTitle,
        .earliest_pos_body = earliestPosBody,

        // Precomputed scores
        .bm25 = weightedBM25,
        .static_rank = static_cast<float>(GetUrlStaticRank(doc.url)),
        .pagerank = info.pagerank_score,
    };

    return ranking::dynamic::GetUrlDynamicRank(features);
}
}  // namespace mithril::ranking
