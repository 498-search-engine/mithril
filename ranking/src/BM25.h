#ifndef RANKING_BM25F_H
#define RANKING_BM25F_H

#include "DocumentMapReader.h"
#include "TermReader.h"
#include "TextPreprocessor.h"

#include <string>

namespace mithril { namespace ranking {

class BM25 {
public:
    BM25(const std::string& index_dir);
    ~BM25() = default;

    // Score a single term for a document
    double ScoreTermForDoc(const data::DocInfo& doc_info, uint32_t docFreq, size_t termFreq);

private:
    // Index stats
    uint32_t doc_count_{0};
    double average_body_length_;

    // BM25F parameters
    double k1_{1.2};
    double b_ = 0.75;

    // Helper methods
    void LoadIndexStats(const std::string& index_dir);
    double CalculateIDF(uint32_t doc_freq) const;

    // Helper to get field lengths from DocInfo
    static uint32_t GetFieldLength(const DocInfo& doc_info, FieldType field);
};

}}  // namespace mithril::ranking

#endif  // RANKING_BM25F_H