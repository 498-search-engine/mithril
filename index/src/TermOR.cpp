#include "TermOR.h"

#include <algorithm>
#include <limits>

namespace mithril {

TermOR::TermOR(std::vector<std::unique_ptr<IndexStreamReader>> readers) : readers_(std::move(readers)) {
    if (readers_.empty()) {
        at_end_ = true;
        return;
    }
    findMinimumReader();
}

bool TermOR::hasNext() const {
    return !at_end_;
}

void TermOR::moveNext() {
    if (at_end_) {
        return;
    }
    // Advance the curr min reader
    readers_[current_min_index_]->moveNext();
    // Find the new minimum
    findMinimumReader();
}

data::docid_t TermOR::currentDocID() const {
    if (at_end_) {
        return std::numeric_limits<data::docid_t>::max();
    }
    return readers_[current_min_index_]->currentDocID();
}

void TermOR::seekToDocID(data::docid_t target_doc_id) {
    if (at_end_) {
        return;
    }
    // Seek all readers to the target
    for (auto& reader : readers_) {
        reader->seekToDocID(target_doc_id);
    }
    // Find the new minimum
    findMinimumReader();
}

void TermOR::findMinimumReader() {
    at_end_ = true;  // Assume we're at the end until we find a valid reader
    data::docid_t min_doc_id = std::numeric_limits<data::docid_t>::max();

    for (size_t i = 0; i < readers_.size(); ++i) {
        if (readers_[i]->hasNext()) {
            data::docid_t doc_id = readers_[i]->currentDocID();
            if (doc_id < min_doc_id) {
                min_doc_id = doc_id;
                current_min_index_ = i;
                at_end_ = false;
            }
        }
    }
}

}  // namespace mithril