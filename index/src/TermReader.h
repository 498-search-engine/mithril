#ifndef INDEX_TERMREADER_H
#define INDEX_TERMREADER_H

#include "IndexStreamReader.h"
#include "PositionIndex.h"
#include "TermDictionary.h"

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mithril {

class TermReader : public IndexStreamReader {
public:
    TermReader(const std::string& index_path, const std::string& term, TermDictionary& term_dict);
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

    // postion specific funcs
    bool hasPositions() const;
    std::vector<uint32_t> currentPositions() const;

private:
    TermDictionary& term_dict_;
    std::string term_;
    std::string index_path_;
    std::ifstream index_file_;
    bool found_term_{false};
    bool at_end_{false};

    // Current posting state
    std::vector<std::pair<uint32_t, uint32_t>> postings_;  // doc_id, freq pairs
    size_t current_posting_index_{0};

    mutable std::shared_ptr<PositionIndex> position_index_;

    bool findTerm(const std::string& term);
    bool findTermWithDict(const std::string& term, const TermDictionary& dictionary);
    static uint32_t decodeVByte(std::istream& in);
};

}  // namespace mithril

#endif  // INDEX_TERMREADER_H
