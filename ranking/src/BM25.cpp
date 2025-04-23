#include "BM25.h"

#include "TermDictionary.h"

#include <fstream>
#include <spdlog/spdlog.h>

namespace mithril { namespace ranking {

BM25::BM25(const std::string& index_dir) {
    LoadIndexStats(index_dir);
}

void BM25::LoadIndexStats(const std::string& index_dir) {
    std::string statsPath = index_dir + "/index_stats.data";
    std::ifstream statsFile(statsPath, std::ios::binary);

    if (!statsFile) {
        spdlog::error("Failed to open index stats file: {}", statsPath);
        throw std::runtime_error("Cannot load index statistics");
    }

    // Read document count
    statsFile.read(reinterpret_cast<char*>(&doc_count_), sizeof(doc_count_));

    // Read field total lengths (BODY, TITLE, URL, DESC)
    uint64_t bodyFieldTotal;
    statsFile.read(reinterpret_cast<char*>(&bodyFieldTotal), sizeof(uint64_t));

    average_body_length_ = doc_count_ > 0 ? static_cast<double>(bodyFieldTotal) / doc_count_ : 0.0;

    spdlog::info("Loaded index stats: {} documents, avg body lengths: [{:.2f}", doc_count_, average_body_length_);
}

double BM25::CalculateIDF(uint32_t doc_freq) const {
    // BM25 IDF formula: log((N-n+0.5)/(n+0.5))
    double N = static_cast<double>(doc_count_);
    double n = static_cast<double>(doc_freq);
    return std::log((N - n + 0.5) / (n + 0.5));
}

uint32_t BM25::GetFieldLength(const DocInfo& doc_info, FieldType field) {
    switch (field) {
    case FieldType::BODY:
        return doc_info.body_length;
    case FieldType::TITLE:
        return doc_info.title_length;
    case FieldType::URL:
        return doc_info.url_length;
    case FieldType::DESC:
        return doc_info.desc_length;
    default:
        return 0;
    }
}

double BM25::ScoreTermForDoc(const data::DocInfo& doc_info, uint32_t docFreq, size_t termFreq) {

    if (termFreq == 0) {
        termFreq = 1;
    }

    if (docFreq == 0) {
        return 0.0;
    }

    double idf = CalculateIDF(docFreq);

    // Get the DocInfo for field lengths
    const DocInfo& docInfo = doc_info;

    // Calculate  term frequency
    double tfCombined = 0.0;


    // If term appears in this field or we're checking the body field
    // (fallback for terms without position/field info)
    // Get field length and avg field length
    uint32_t fieldLength = GetFieldLength(docInfo, FieldType::BODY);
    double averageFieldLength = average_body_length_;

    // BM25 field normalization
    double normFactor = 1.0;
    if (averageFieldLength > 0) {
        normFactor = (1.0 - b_) + b_ * (fieldLength / averageFieldLength);
    }

    // Add field's contribution to combined term frequency
    if (normFactor > 0) {
        tfCombined = static_cast<double>(termFreq) / normFactor;
    }

    // BM25 saturation function
    double score = idf * (tfCombined * (k1_ + 1)) / (tfCombined + k1_);
    return std::log(score);
}

}}  // namespace mithril::ranking