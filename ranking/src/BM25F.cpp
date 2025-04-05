#include "BM25F.h"

#include <fstream>
#include <spdlog/spdlog.h>

namespace mithril { namespace ranking {

BM25F::BM25F(const std::string& index_dir) : config_("BM25.conf"), doc_reader_(index_dir) {

    // Create position index for accessing field flags
    position_index_ = std::make_shared<PositionIndex>(index_dir);

    loadIndexStats(index_dir);
    loadParameters();
}

void BM25F::loadParameters() {
    // Load BM25F parameters from config
    k1_ = config_.GetDouble("k1", 1.2);

    // b values for each field (controls field length normalization)
    b_[static_cast<size_t>(FieldType::BODY)] = config_.GetDouble("b_body", 0.75);
    b_[static_cast<size_t>(FieldType::TITLE)] = config_.GetDouble("b_title", 0.75);
    b_[static_cast<size_t>(FieldType::URL)] = config_.GetDouble("b_url", 0.75);
    b_[static_cast<size_t>(FieldType::DESC)] = config_.GetDouble("b_desc", 0.75);

    // Weight for each field
    weights_[static_cast<size_t>(FieldType::BODY)] = config_.GetDouble("weight_body", 1.0);
    weights_[static_cast<size_t>(FieldType::TITLE)] = config_.GetDouble("weight_title", 3.0);
    weights_[static_cast<size_t>(FieldType::URL)] = config_.GetDouble("weight_url", 1.0);
    weights_[static_cast<size_t>(FieldType::DESC)] = config_.GetDouble("weight_desc", 1.5);

    spdlog::info("BM25F parameters loaded: k1={}, weights=[{},{},{},{}], b=[{},{},{},{}]",
                 k1_,
                 weights_[0],
                 weights_[1],
                 weights_[2],
                 weights_[3],
                 b_[0],
                 b_[1],
                 b_[2],
                 b_[3]);
}

void BM25F::loadIndexStats(const std::string& index_dir) {
    std::string stats_path = index_dir + "/index_stats.data";
    std::ifstream stats_file(stats_path, std::ios::binary);

    if (!stats_file) {
        spdlog::error("Failed to open index stats file: {}", stats_path);
        throw std::runtime_error("Cannot load index statistics");
    }

    // Read document count
    stats_file.read(reinterpret_cast<char*>(&doc_count_), sizeof(doc_count_));

    // Read field total lengths (BODY, TITLE, URL, DESC)
    uint64_t field_totals[4];
    stats_file.read(reinterpret_cast<char*>(&field_totals[0]), sizeof(uint64_t));
    stats_file.read(reinterpret_cast<char*>(&field_totals[1]), sizeof(uint64_t));
    stats_file.read(reinterpret_cast<char*>(&field_totals[2]), sizeof(uint64_t));
    stats_file.read(reinterpret_cast<char*>(&field_totals[3]), sizeof(uint64_t));

    // Calculate average field lengths
    for (int i = 0; i < 4; i++) {
        avg_field_lengths_[i] = doc_count_ > 0 ? static_cast<double>(field_totals[i]) / doc_count_ : 0.0;
    }

    spdlog::info("Loaded index stats: {} documents, avg field lengths: [{:.2f}, {:.2f}, {:.2f}, {:.2f}]",
                 doc_count_,
                 avg_field_lengths_[0],
                 avg_field_lengths_[1],
                 avg_field_lengths_[2],
                 avg_field_lengths_[3]);
}

double BM25F::calculateIDF(uint32_t doc_freq) const {
    // BM25 IDF formula: log((N-n+0.5)/(n+0.5))
    double N = static_cast<double>(doc_count_);
    double n = static_cast<double>(doc_freq);
    return std::log((N - n + 0.5) / (n + 0.5));
}

uint32_t BM25F::getFieldLength(const DocInfo& doc_info, FieldType field) const {
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

double BM25F::scoreTermForDoc(TermReader& term_reader, data::docid_t doc_id) {
    // Position term reader at this document
    term_reader.seekToDocID(doc_id);
    if (!term_reader.hasNext() || term_reader.currentDocID() != doc_id) {
        return 0.0;  // Term not in this document
    }

    // Get document information
    auto doc_opt = doc_reader_.getDocument(doc_id);
    if (!doc_opt)
        return 0.0;  // Document not found

    // Calculate IDF component
    double idf = calculateIDF(term_reader.getDocumentCount());

    // Get term frequency
    uint32_t term_freq = term_reader.currentFrequency();

    // Get field flags using the PositionIndex
    uint8_t field_flags = 0;
    if (term_reader.hasPositions()) {
        field_flags = position_index_->getFieldFlags(term_reader.getTerm(), doc_id);
    }

    // Get the DocInfo for field lengths
    const DocInfo& doc_info = *(reinterpret_cast<const DocInfo*>(&(*doc_opt)));

    // Calculate weighted term frequency across fields
    double tf_combined = 0.0;

    for (size_t i = 0; i < 4; ++i) {
        FieldType field = static_cast<FieldType>(i);
        uint8_t flag = fieldTypeToFlag(field);

        // If term appears in this field or we're checking the body field
        // (fallback for terms without position/field info)
        if ((field_flags & flag) || (field_flags == 0 && field == FieldType::BODY)) {
            // Get field length and avg field length
            uint32_t field_len = getFieldLength(doc_info, field);
            double avg_field_len = avg_field_lengths_[i];

            // Estimate field-specific frequency
            double field_freq = term_freq;
            if (field_flags != 0) {
                // If we know field distribution, distribute frequency proportionally
                int num_fields = __builtin_popcount(field_flags);
                field_freq = term_freq / num_fields;
            }

            // BM25F field normalization
            double norm_factor = 1.0;
            if (avg_field_len > 0) {
                norm_factor = (1.0 - b_[i]) + b_[i] * (field_len / avg_field_len);
            }

            // Add field's contribution to combined term frequency
            if (norm_factor > 0) {
                tf_combined += weights_[i] * field_freq / norm_factor;
            }
        }
    }

    // BM25 saturation function
    double score = idf * (tf_combined * (k1_ + 1)) / (tf_combined + k1_);
    return score;
}

double BM25F::scoreForDoc(const std::vector<std::unique_ptr<TermReader>>& terms, data::docid_t doc_id) {
    double total_score = 0.0;
    for (const auto& term : terms) {
        total_score += scoreTermForDoc(*term, doc_id);
    }
    return total_score;
}

float BM25F::getPageRankScore(data::docid_t doc_id) const {
    auto doc_opt = doc_reader_.getDocument(doc_id);
    if (!doc_opt)
        return 0.0f;

    // Access the DocInfo for pagerank score
    const DocInfo& doc_info = *(reinterpret_cast<const DocInfo*>(&(*doc_opt)));
    return doc_info.pagerank_score;
}

std::optional<data::Document> BM25F::getDocument(data::docid_t doc_id) const {
    return doc_reader_.getDocument(doc_id);
}

double BM25F::getCombinedScore(const std::vector<std::unique_ptr<TermReader>>& terms,
                               data::docid_t doc_id,
                               double bm25f_weight,
                               double pagerank_weight) {
    double bm25f_score = scoreForDoc(terms, doc_id);
    float pagerank_score = getPageRankScore(doc_id);

    // Simple linear combination
    return bm25f_weight * bm25f_score + pagerank_weight * pagerank_score;
}

}}  // namespace mithril::ranking