#ifndef RANKING_RANKER_H
#define RANKING_RANKER_H
#include "core/config.h"

namespace mithril::ranking::dynamic {
static inline core::Config Config = core::Config("dynamicranker.conf");

struct RankerFeatures {
    // Boolean presence flags
    bool query_in_url;
    bool query_in_title;
    bool query_in_description;
    bool query_in_body;
    bool query_in_order;

    // Percentage features
    float percent_query_url;
    float percent_query_title;
    float percent_query_description;
    float percent_query_body;

    // Position features (normalized 0-1)
    float earliest_pos_url;
    float earliest_pos_title;
    float earliest_pos_body;

    // Frequency/proximity features
    float body_term_freq;  // Normalized term frequency
    float short_spans;     // Normalized span count

    // Precomputed scores
    float bm25;
    float static_rank;
    float pagerank;
};

struct RankerWeights {
    float bm25 = Config.GetFloat("bm25");
    float query_in_title = Config.GetFloat("query_in_title");
    float query_in_url = Config.GetFloat("query_in_url");
    float query_in_description = Config.GetFloat("query_in_description");
    float query_in_body = Config.GetFloat("query_in_body");
    float query_in_order = Config.GetFloat("query_in_order");
    float short_spans = Config.GetFloat("short_spans");
    float body_term_freq = Config.GetFloat("body_term_freq");
    float percent_query_url = Config.GetFloat("percent_query_url");
    float percent_query_title = Config.GetFloat("percent_query_title");
    float percent_query_description = Config.GetFloat("percent_query_description");
    float percent_query_body = Config.GetFloat("percent_query_body");
    float earliest_pos_title = Config.GetFloat("earliest_pos_title");
    float earliest_pos_url = Config.GetFloat("earliest_pos_url");
    float earliest_pos_body = Config.GetFloat("earliest_pos_body");
    float static_rank = Config.GetFloat("static_rank");
    float pagerank = Config.GetFloat("pagerank");
};

static inline const RankerWeights Weights;

static inline const float MinScore = 0.0F;
static inline const float MaxScore =
    Weights.bm25 + Weights.query_in_title + Weights.query_in_url + Weights.query_in_description +
    Weights.query_in_body + Weights.query_in_order + Weights.short_spans + Weights.body_term_freq +
    Weights.percent_query_url + Weights.percent_query_title + Weights.percent_query_description +
    Weights.percent_query_body + Weights.earliest_pos_title + Weights.earliest_pos_url + Weights.earliest_pos_body +
    Weights.static_rank + Weights.pagerank;
static inline const float ScoreRange = MaxScore - MinScore;

float GetUrlDynamicRank(const RankerFeatures& features);

};  // namespace mithril::ranking::dynamic
#endif