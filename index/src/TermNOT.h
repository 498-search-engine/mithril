#ifndef INDEX_TERMNOT_H
#define INDEX_TERMNOT_H

#include "IndexStreamReader.h"
#include "TermReader.h"

#include <memory>
#include <string>
#include <vector>

namespace mithril {

class TermNOT : public IndexStreamReader {
public:
    TermNOT(const std::string& index_path, const std::string& term, data::docid_t max_docid);

    // IndexStreamReader implementation
    bool hasNext() const override;
    void moveNext() override;
    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

    // Additional functions similar to TermReader
    uint32_t currentFrequency() const;
    std::string getTerm() const { return "NOT " + term_reader_->getTerm(); }
    
    // Position specific functions
    bool hasPositions() const;
    std::vector<uint32_t> currentPositions() const;

private:
    void advanceToNextNonMatchingDoc();

    std::unique_ptr<TermReader> term_reader_;
    data::docid_t max_docid_;
    data::docid_t current_docid_;
};

}  // namespace mithril

#endif  // INDEX_TERMNOT_H
