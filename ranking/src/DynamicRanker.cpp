#include "DynamicRanker.h"

#include "spdlog/sinks/basic_file_sink.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking::dynamic {
namespace {
std::shared_ptr<spdlog::logger> rankerLogger = spdlog::basic_logger_mt("ranker_logger", "ranker.log");

void Log(const RankerFeatures& features, float total, uint32_t normalizedScore) {
    rankerLogger->flush_on(spdlog::level::trace);

    rankerLogger->info("Dynamic ranking components:");
    rankerLogger->info("- BM25: {:.4f} ({:.2f}*{:.2f})", Weights.bm25 * features.bm25, Weights.bm25, features.bm25);

    rankerLogger->info("- Title: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f})",
                       features.query_in_title,
                       Weights.query_in_title,
                       features.query_in_title,
                       Weights.percent_query_title * features.percent_query_title,
                       Weights.percent_query_title,
                       features.percent_query_title);

    rankerLogger->info("- URL: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f})",
                       features.query_in_url,
                       Weights.query_in_url,
                       features.query_in_url,
                       Weights.percent_query_url * features.percent_query_url,
                       Weights.percent_query_url,
                       features.percent_query_url);

    rankerLogger->info("- Description: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f})",
                       features.query_in_description,
                       Weights.query_in_description,
                       features.query_in_description,
                       Weights.percent_query_description * features.percent_query_description,
                       Weights.percent_query_description,
                       features.percent_query_description);

    rankerLogger->info("- Body: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f}), freq={:.2f} ({:.2f}*{:.2f})",
                       features.query_in_body,
                       Weights.query_in_body,
                       features.query_in_body,
                       Weights.percent_query_body * features.percent_query_body,
                       Weights.percent_query_body,
                       features.percent_query_body,
                       Weights.body_term_freq * features.body_term_freq,
                       Weights.body_term_freq,
                       features.body_term_freq);

    rankerLogger->info("- Proximity: order={} ({:.2f}*{}), spans={:.2f} ({:.2f}*{:.2f})",
                       features.query_in_order,
                       Weights.query_in_order,
                       features.query_in_order,
                       Weights.short_spans * features.short_spans,
                       Weights.short_spans,
                       features.short_spans);

    rankerLogger->info("- Positions: title={:.2f}, url={:.2f}, body={:.2f}",
                       (1.0F - features.earliest_pos_title) * Weights.earliest_pos_title,
                       (1.0F - features.earliest_pos_url) * Weights.earliest_pos_url,
                       (1.0F - features.earliest_pos_body) * Weights.earliest_pos_body);

    rankerLogger->info("- Precomputed ranking: static={:.2f}, pagerank={:.2f}",
                       Weights.static_rank * features.static_rank,
                       Weights.pagerank * features.pagerank);

    rankerLogger->info("Total dynamic score: {} ({:.4f})", normalizedScore, total);
}
};  // namespace

uint32_t GetUrlDynamicRank(const RankerFeatures& features) {
    float score = 0.0F;

    // Content relevance features
    score += Weights.bm25 * features.bm25;
    score += Weights.query_in_title * static_cast<float>(features.query_in_title);
    score += Weights.query_in_url * static_cast<float>(features.query_in_url);
    score += Weights.query_in_order * static_cast<float>(features.query_in_order);
    score += Weights.short_spans * features.short_spans;
    score += Weights.body_term_freq * features.body_term_freq;
    score += Weights.query_in_body * static_cast<float>(features.query_in_body);
    score += Weights.percent_query_body * features.percent_query_body;

    // Early occurrence bonuses (invert position)
    score += Weights.earliest_pos_title * (1.0F - features.earliest_pos_title);
    score += Weights.earliest_pos_url * (1.0F - features.earliest_pos_url);
    score += Weights.earliest_pos_body * (1.0F - features.earliest_pos_body);

    // Authority signals
    score += Weights.static_rank * features.static_rank;
    score += Weights.pagerank * features.pagerank;

    // Secondary content features
    score += Weights.query_in_description * static_cast<float>(features.query_in_description);
    score += Weights.percent_query_title * features.percent_query_title;
    score += Weights.percent_query_url * features.percent_query_url;
    score += Weights.percent_query_description * features.percent_query_description;

    uint32_t finalScore = static_cast<uint32_t>(((score - MinScore) / ScoreRange) * 10000);
    if (finalScore > 3000) {
#if LOGGING == 1
        Log(features, score, finalScore);
#endif
    }

    return finalScore;
}
}  // namespace mithril::ranking::dynamic