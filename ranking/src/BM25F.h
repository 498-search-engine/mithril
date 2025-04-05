#ifndef RANKING_BM25F_H
#define RANKING_BM25F_H

#include "DocumentMapReader.h"
#include "PositionIndex.h"
#include "TermReader.h"
#include "TextPreprocessor.h"
#include "core/config.h"

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
/*
 * REWRITE THIS REWRITE THSI REWRITE THIS @ANUBHAV
 */

namespace mithril { namespace ranking {

class BM25F {
public:
    BM25F(const std::string& index_dir);
    ~BM25F() = default;

    // Score a single term for a document
    double scoreTermForDoc(TermReader& term_reader, data::docid_t doc_id);

    // Score all query terms for a document
    double scoreForDoc(const std::vector<std::unique_ptr<TermReader>>& terms, data::docid_t doc_id);

    // Get raw PageRank score for a document
    float getPageRankScore(data::docid_t doc_id) const;

    // Get document information - useful for display
    std::optional<data::Document> getDocument(data::docid_t doc_id) const;

    // Combined scoring with BM25F and PageRank
    double getCombinedScore(const std::vector<std::unique_ptr<TermReader>>& terms,
                            data::docid_t doc_id,
                            double bm25f_weight = 0.8,
                            double pagerank_weight = 0.2);

private:
    // Configuration and document readers
    core::Config config_;
    DocumentMapReader doc_reader_;
    std::shared_ptr<PositionIndex> position_index_;

    // Index stats
    uint32_t doc_count_{0};
    std::array<double, 4> avg_field_lengths_{
        {0.0, 0.0, 0.0, 0.0}
    };

    // BM25F parameters
    double k1_{1.2};
    std::array<double, 4> b_{
        {0.75, 0.75, 0.75, 0.75}
    };
    std::array<double, 4> weights_{
        {1.0, 3.0, 1.0, 1.5}
    };

    // Helper methods
    void loadIndexStats(const std::string& index_dir);
    void loadParameters();
    double calculateIDF(uint32_t doc_freq) const;

    // Helper to get field lengths from DocInfo
    uint32_t getFieldLength(const DocInfo& doc_info, FieldType field) const;
};

}}  // namespace mithril::ranking

#endif  // RANKING_BM25F_H