#ifndef RANKING_RANKER_H
#define RANKING_RANKER_H
#include "core/config.h"
#include <vector>
namespace mithril::ranking::dynamic {
static inline core::Config Config = core::Config("dynamicranker.conf");

struct RankerFeatures {
    // Boolean presence flags
    bool query_in_url;
    bool query_in_title;
    bool query_in_description;
    bool query_in_body;

    // Query Coverage percentage features
    float coverage_percent_query_url;
    float coverage_percent_query_title;
    float coverage_percent_query_description;

    float order_sensitive_title;
    // Query Density percentage features
    float density_percent_query_url;
    float density_percent_query_title;
    float density_percent_query_description;

    // Position features (normalized 0-1)
    float earliest_pos_title;
    float earliest_pos_body;

    // Precomputed scores
    float bm25;
    float static_rank;
    float pagerank;
};

struct RankerWeights {
    // Boolean presence flags
    float query_in_title = Config.GetFloat("query_in_title");
    float query_in_url = Config.GetFloat("query_in_url");
    float query_in_description = Config.GetFloat("query_in_description");
    float query_in_body = Config.GetFloat("query_in_body");

    // Query Coverage percentage features
    float coverage_percent_query_url = Config.GetFloat("coverage_percent_query_url");
    float coverage_percent_query_title = Config.GetFloat("coverage_percent_query_title");
    float coverage_percent_query_description = Config.GetFloat("coverage_percent_query_description");

    float order_sensitive_title = Config.GetFloat("order_sensitive_title");

    // Query Density percentage features
    float density_percent_query_url = Config.GetFloat("density_percent_query_url");
    ;
    float density_percent_query_title = Config.GetFloat("density_percent_query_title");
    ;
    float density_percent_query_description = Config.GetFloat("density_percent_query_description");
    ;

    // Position features (normalized 0-1)
    float earliest_pos_title = Config.GetFloat("earliest_pos_title");
    float earliest_pos_body = Config.GetFloat("earliest_pos_body");

    // Precomputed scores
    float bm25 = Config.GetFloat("bm25");
    float static_rank = Config.GetFloat("static_rank");
    float pagerank = Config.GetFloat("pagerank");
};

static inline const RankerWeights Weights;

static inline const float MinScore = 0.0F;
static inline const float MaxScore =
    Weights.query_in_title + Weights.query_in_url + Weights.query_in_description + Weights.query_in_body +
    Weights.coverage_percent_query_url + Weights.coverage_percent_query_title +
    Weights.coverage_percent_query_description + Weights.order_sensitive_title + Weights.density_percent_query_url +
    Weights.density_percent_query_title + Weights.density_percent_query_description + Weights.earliest_pos_title +
    Weights.earliest_pos_body + Weights.bm25 + Weights.static_rank + Weights.pagerank;
static inline const float ScoreRange = MaxScore - MinScore;

uint32_t GetUrlDynamicRank(const RankerFeatures& features);
float OrderedMatchScore(const std::vector<std::pair<std::string, int>>& qTokens,
                        const std::vector<std::string>& tTokens);
}  // namespace mithril::ranking::dynamic
#endif