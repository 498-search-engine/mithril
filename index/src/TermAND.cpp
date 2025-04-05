#include "TermAND.h"

#include "TermReader.h"

#include <cassert>
#include <limits>

namespace mithril {

TermAND::TermAND(std::vector<std::unique_ptr<IndexStreamReader>> readers) : readers_(std::move(readers)) {
    if (readers_.empty()) {
        at_end_ = true;
        return;
    }
    sortReadersByFrequency();

    if (!findNextMatch()) {
        at_end_ = true;
    }
}

bool TermAND::hasNext() const {
    return !at_end_;
}

void TermAND::moveNext() {
    if (at_end_) {
        return;
    }

    // All readers are currently at current_doc_id_
    // Advance the first reader to look for the next potential match
    readers_[0]->moveNext();
    // Find the next document that contains all terms
    if (!findNextMatch()) {
        at_end_ = true;
    }
}

data::docid_t TermAND::currentDocID() const {
    if (at_end_) {
        return std::numeric_limits<data::docid_t>::max();
    }
    return current_doc_id_;
}

void TermAND::seekToDocID(data::docid_t target_doc_id) {
    if (at_end_) {
        return;
    }

    // Seek the first reader to the target document
    readers_[0]->seekToDocID(target_doc_id);
    // Find the next match starting from this position
    if (!findNextMatch()) {
        at_end_ = true;
    }
}

bool TermAND::findNextMatch() {
    // early exit if any reader is at end
    for (const auto& reader : readers_) {
        if (!reader->hasNext()) {
            return false;
        }
    }

    // impl of the efficient AND algorithm from sldies
    // Skip as much as possible by always advancing to the highest docID
    while (true) {
        // Get the first reader's current document ID
        data::docid_t candidate_doc_id = readers_[0]->currentDocID();
        bool all_match = true;

        // Check if all other readers have this document
        for (size_t i = 1; i < readers_.size(); ++i) {
            // Seek to the candidate document or beyond
            readers_[i]->seekToDocID(candidate_doc_id);

            // If a reader doesn't have more documents or is past our candidate
            if (!readers_[i]->hasNext() || readers_[i]->currentDocID() > candidate_doc_id) {
                all_match = false;

                // If a reader is past our candidate, we need to seek the first reader
                // to the new higher docID and start over
                if (readers_[i]->hasNext()) {
                    data::docid_t new_candidate = readers_[i]->currentDocID();
                    readers_[0]->seekToDocID(new_candidate);

                    // If first reader can't reach this document, we're done
                    if (!readers_[0]->hasNext() || readers_[0]->currentDocID() > new_candidate) {
                        continue;  // Will pick up the new candidate in the next iteration
                    }
                } else {
                    // One reader is exhausted, no more matches possible
                    return false;
                }
                break;
            }
        }

        // If all readers matched on the same document, we have a match
        if (all_match) {
            current_doc_id_ = candidate_doc_id;
            return true;
        }

        // If we reach here, we need to try again with the new candidate
        if (!readers_[0]->hasNext()) {
            return false;
        }
    }
}

void TermAND::sortReadersByFrequency() {
    // Sort readers by document frequency (if available) for query optimization
    // This puts the rarest terms first, which minimizes the number of documents we need to check

    std::stable_sort(readers_.begin(),
                     readers_.end(),
                     [](const std::unique_ptr<IndexStreamReader>& a, const std::unique_ptr<IndexStreamReader>& b) {
                         // Try to cast to TermReader to get frequency information
                         const TermReader* term_reader_a = dynamic_cast<const TermReader*>(a.get());
                         const TermReader* term_reader_b = dynamic_cast<const TermReader*>(b.get());

                         // If frequency information is available, use it for init ranking
                         if (term_reader_a && term_reader_b) {
                             return term_reader_a->getDocumentCount() < term_reader_b->getDocumentCount();
                         }

                         // Otherwise, default ordering
                         return false;
                     });
}

IndexStreamReader* TermAND::get(std::size_t i) {
    return i < readers_.size() ? readers_[i].get() : nullptr;
}

std::size_t TermAND::numReaders() const {
    return readers_.size();
}

}  // namespace mithril
