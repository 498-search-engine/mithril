#ifndef INDEX_TERMOR_H
#define INDEX_TERMOR_H

#include "IndexStreamReader.h"

#include <memory>
#include <vector>

namespace mithril {

class TermOR : public IndexStreamReader {
public:
    explicit TermOR(std::vector<std::unique_ptr<IndexStreamReader>> readers);
    ~TermOR() override = default;

    bool hasNext() const override;
    void moveNext() override;
    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

private:
    std::vector<std::unique_ptr<IndexStreamReader>> readers_;
    // Index of the reader with the current minimum docID
    size_t current_min_index_{0};
    // Flag indicating if we've reached the end of all readers
    bool at_end_{false};
    // Helper to find the reader with the minimum docID
    void findMinimumReader();
};

}  // namespace mithril

#endif  // INDEX_TERMOR_H
