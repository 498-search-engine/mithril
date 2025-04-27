#ifndef INDEX_TERMREADER_H
#define INDEX_TERMREADER_H

#include "IndexStreamReader.h"
#include "PositionIndex.h"
#include "PostingBlock.h"
#include "TermDictionary.h"
#include "core/mem_map_file.h"

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mithril {

class TermReader : public IndexStreamReader {
public:
    TermReader(const std::string& index_path,
               const std::string& term,
               const core::MemMapFile& index_file,
               TermDictionary& term_dict,
               PositionIndex& position_index);
    ~TermReader();

    // ISR
    bool hasNext() const override;
    void moveNext() override;
    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

    // term specific funcs
    uint32_t currentFrequency() const;
    std::string getTerm() const { return term_; }
    uint32_t getDocumentCount() const { return postings_.size(); }

    double getAverageFrequency() const;

    // postion specific funcs
    bool hasPositions() const;
    std::vector<uint16_t> currentPositions() const;

private:
    TermDictionary& term_dict_;
    std::string term_;
    std::string index_path_;
    std::string index_dir_;
    const core::MemMapFile& index_file_;
    bool found_term_{false};
    bool at_end_{false};

    // Current posting state
    std::vector<core::Pair<uint32_t, uint32_t>> postings_;  // doc_id, freq pairs
    size_t current_posting_index_{0};
    std::vector<SyncPoint> sync_points_;

    PositionIndex& position_index_;

    mutable double avg_frequency_{0.0};
    mutable bool avg_frequency_computed_{false};

    bool findTerm(const std::string& term);
    bool findTermWithDict(const std::string& term, const TermDictionary& dictionary);
    uint32_t decodeVByte(const char*& ptr);
};

}  // namespace mithril

#endif  // INDEX_TERMREADER_H
