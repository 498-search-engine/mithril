#ifndef INDEX_TERMREADER_H
#define INDEX_TERMREADER_H

#include "IndexStreamReader.h"
#include "TermDictionary.h"

#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace mithril {

class TermReader : public IndexStreamReader {
public:
    TermReader(const std::string& index_path, const std::string& term);
    ~TermReader();

    // ISR
    bool hasNext() const override;
    void moveNext() override;
    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

    // term specific func
    uint32_t currentFrequency() const;
    std::vector<uint32_t> currentPositions() const;
    std::string getTerm() const { return term_; }
    uint32_t getDocumentCount() const { return postings_.size(); }

private:
    std::string term_;
    std::string index_path_;
    std::ifstream index_file_;

    bool found_term_{false};
    bool at_end_{false};

    // Current posting state
    std::vector<std::pair<uint32_t, uint32_t>> postings_;  // doc_id, freq pairs
    std::streampos positions_start_pos_{0};                // File postn where positions start
    uint32_t positions_count_{0};
    size_t current_posting_index_{0};

    bool findTerm(const std::string& term);
    bool findTermWithDict(const std::string& term, const TermDictionary& dictionary);

    static uint32_t decodeVByte(std::istream& in);

    std::vector<uint32_t> loadPositions(size_t posting_index) const;
};

}  // namespace mithril

#endif  // INDEX_TERMREADER_H
