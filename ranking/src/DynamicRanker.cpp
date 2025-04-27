#include "DynamicRanker.h"

#include "spdlog/sinks/basic_file_sink.h"

#include <spdlog/spdlog.h>

#define LOGGING 1

namespace mithril::ranking::dynamic {
namespace {
std::shared_ptr<spdlog::logger> rankerLogger = spdlog::basic_logger_mt("ranker_logger", "ranker.log");

void Log(const RankerFeatures& features, float total, uint32_t normalizedScore) {
    rankerLogger->flush_on(spdlog::level::trace);

    rankerLogger->info("Dynamic ranking components:");
    rankerLogger->info("- BM25: {:.4f} ({:.2f}*{:.2f})", Weights.bm25 * features.bm25, Weights.bm25, features.bm25);

    rankerLogger->info("- Title: presence={} ({:.2f}*{}), coverage={:.4f} ({:.2f}*{:.2f}), density={:.4f}, "
                       "({:.2f}*{:.2f}), order sensitive={:.4f} ({:.2f}*{:.2f})",
                       features.query_in_title,
                       Weights.query_in_title,
                       features.query_in_title,
                       Weights.coverage_percent_query_title * features.coverage_percent_query_title,
                       Weights.coverage_percent_query_title,
                       features.coverage_percent_query_title,
                       Weights.density_percent_query_title * features.density_percent_query_title,
                       Weights.density_percent_query_title,
                       features.density_percent_query_title,
                       Weights.order_sensitive_title * features.order_sensitive_title,
                       Weights.order_sensitive_title,
                       features.order_sensitive_title);

    rankerLogger->info(
        "- URL: presence={} ({:.2f}*{}), coverage={:.4f} ({:.2f}*{:.2f}), density={:.4f} ({:.2f}*{:.2f})",
        features.query_in_url,
        Weights.query_in_url,
        features.query_in_url,
        Weights.coverage_percent_query_url * features.coverage_percent_query_url,
        Weights.coverage_percent_query_url,
        features.coverage_percent_query_url,
        Weights.density_percent_query_url * features.density_percent_query_url,
        Weights.density_percent_query_url,
        features.density_percent_query_url);

    rankerLogger->info(
        "- Description: presence={} ({:.2f}*{}), coverage={:.4f} ({:.2f}*{:.2f}), density={:.4f} ({:.2f}*{:.2f})",
        features.query_in_description,
        Weights.query_in_description,
        features.query_in_description,
        Weights.coverage_percent_query_description * features.coverage_percent_query_description,
        Weights.coverage_percent_query_description,
        features.coverage_percent_query_description,
        Weights.density_percent_query_description * features.density_percent_query_description,
        Weights.density_percent_query_description,
        features.density_percent_query_description);

    rankerLogger->info(
        "- Body: presence={} ({:.2f}*{})", features.query_in_body, Weights.query_in_body, features.query_in_body);

    rankerLogger->info("- Positions: title={:.4f}, body={:.4f}",
                       (features.earliest_pos_title) * Weights.earliest_pos_title,
                       (features.earliest_pos_body) * Weights.earliest_pos_body);

    rankerLogger->info("- Precomputed ranking: static={:.4f}, pagerank={:.4f}",
                       Weights.static_rank * features.static_rank,
                       Weights.pagerank * features.pagerank);

    rankerLogger->info("Total dynamic score: {} ({:.4f})", normalizedScore, total);
}
};  // namespace


float OrderedMatchScore(const std::vector<core::Pair<std::string, int>>& qTokens,
                        const std::vector<std::string>& tTokens) {
    auto startsWith = [](const std::string& prefix, const std::string& word) -> bool {
        return word.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), word.begin());
    };

    int qLen = (int)qTokens.size();
    int qIdx = 0;

    for (const auto& token : tTokens) {
        std::string loweredToken;
        for (char c : token) {
            loweredToken += std::tolower(c);
        }

        if (qIdx < qLen && startsWith(loweredToken, qTokens[qIdx].first)) {
            qIdx++;
        }
    }

    return (qLen > 0) ? static_cast<float>(qIdx) / static_cast<float>(qLen) : 0.0F;
}

uint32_t GetUrlDynamicRank(const RankerFeatures& features) {
    float score = 0.0F;

    // Core content relevance
    score += Weights.bm25 * features.bm25;
    score += Weights.query_in_title * static_cast<float>(features.query_in_title);
    score += Weights.query_in_url * static_cast<float>(features.query_in_url);
    score += Weights.query_in_description * static_cast<float>(features.query_in_description);
    score += Weights.query_in_body * static_cast<float>(features.query_in_body);

    // Title signals
    score += Weights.coverage_percent_query_title * features.coverage_percent_query_title;
    score += Weights.density_percent_query_title * features.density_percent_query_title;
    score += Weights.order_sensitive_title * features.order_sensitive_title;

    // URL signals
    score += Weights.coverage_percent_query_url * features.coverage_percent_query_url;
    score += Weights.density_percent_query_url * features.density_percent_query_url;

    // Description signals
    score += Weights.coverage_percent_query_description * features.coverage_percent_query_description;
    score += Weights.density_percent_query_description * features.density_percent_query_description;

    // Positional bonuses (inverted so earlier = higher score)
    score += Weights.earliest_pos_title * (features.earliest_pos_title);
    score += Weights.earliest_pos_body * (features.earliest_pos_body);

    // Precomputed/authority features
    score += Weights.static_rank * features.static_rank;
    score += Weights.pagerank * features.pagerank;

    uint32_t finalScore = static_cast<uint32_t>(((score - MinScore) / ScoreRange) * 10000);
    if (finalScore > 3000) {
#if LOGGING == 1
        Log(features, score, finalScore);
#endif
    }

    return finalScore;
}

}  // namespace mithril::ranking::dynamic