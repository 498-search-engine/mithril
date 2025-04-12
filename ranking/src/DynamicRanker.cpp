#include "DynamicRanker.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking::dynamic {
namespace {
void Log(const RankerFeatures& features, float total) {
    spdlog::debug("Dynamic ranking components:");
    spdlog::debug("- BM25: {:.4f} ({:.2f}*{:.2f})", Weights.bm25 * features.bm25, Weights.bm25, features.bm25);

    spdlog::debug("- Title: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f})",
                  features.query_in_title,
                  Weights.query_in_title,
                  features.query_in_title,
                  Weights.percent_query_title * features.percent_query_title,
                  Weights.percent_query_title,
                  features.percent_query_title);

    spdlog::debug("- URL: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f})",
                  features.query_in_url,
                  Weights.query_in_url,
                  features.query_in_url,
                  Weights.percent_query_url * features.percent_query_url,
                  Weights.percent_query_url,
                  features.percent_query_url);

    spdlog::debug("- Body: presence={} ({:.2f}*{}), coverage={:.2f} ({:.2f}*{:.2f}), freq={:.2f} ({:.2f}*{:.2f})",
                  features.query_in_body,
                  Weights.query_in_body,
                  features.query_in_body,
                  Weights.percent_query_body * features.percent_query_body,
                  Weights.percent_query_body,
                  features.percent_query_body,
                  Weights.body_term_freq * features.body_term_freq,
                  Weights.body_term_freq,
                  features.body_term_freq);

    spdlog::debug("- Proximity: order={} ({:.2f}*{}), spans={:.2f} ({:.2f}*{:.2f})",
                  features.query_in_order,
                  Weights.query_in_order,
                  features.query_in_order,
                  Weights.short_spans * features.short_spans,
                  Weights.short_spans,
                  features.short_spans);

    spdlog::debug("- Positions: title={:.2f}, url={:.2f}, body={:.2f}",
                  (1.0F - features.earliest_pos_title) * Weights.earliest_pos_title,
                  (1.0F - features.earliest_pos_url) * Weights.earliest_pos_url,
                  (1.0F - features.earliest_pos_body) * Weights.earliest_pos_body);

    spdlog::debug("- Precomputed ranking: static={:.2f}, pagerank={:.2f}",
                  Weights.static_rank * features.static_rank,
                  Weights.pagerank * features.pagerank);

    spdlog::debug("Total dynamic score: {:.4f}", total);
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

    if (SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG) {
        Log(features, score);
    }

    return static_cast<uint32_t>(((score - MinScore) / ScoreRange) * 10000);
}
}  // namespace mithril::ranking::dynamic