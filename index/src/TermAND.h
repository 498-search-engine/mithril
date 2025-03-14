#ifndef MITHRIL_TERMAND_H
#define MITHRIL_TERMAND_H

#include "IndexStreamReader.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace mithril {

class TermAND : public IndexStreamReader {
public:
    explicit TermAND(std::vector<std::unique_ptr<IndexStreamReader>> readers);
    ~TermAND() override = default;

    bool hasNext() const override;
    void moveNext() override;
    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

private:
    std::vector<std::unique_ptr<IndexStreamReader>> readers_;
    // Cached document ID for the current match
    data::docid_t current_doc_id_{0};
    // Flag indicating if we've reached the end
    bool at_end_{false};
    // Finds the next document where all terms appear
    bool findNextMatch();
    // Sort readers by ascending document frequency (optimization)
    void sortReadersByFrequency();
};

}  // namespace mithril

#endif  // MITHRIL_TERMAND_H
