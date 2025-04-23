#ifndef INDEX_NOTISR_H
#define INDEX_NOTISR_H

#include "IndexStreamReader.h"
#include <memory>

namespace mithril {

class NotISR : public IndexStreamReader {
public:
    explicit NotISR(std::unique_ptr<IndexStreamReader> reader_in, size_t document_count_in)
        : reader_(std::move(reader_in)), doc_count_(document_count_in) {
        // Start at document ID 0 (before first document)
        current_doc_id_ = 0;
        
        // If reader is empty, all documents are included
        if (!reader_->hasNext()) {
            return;
        }
        
        // Find first valid document
        moveNext();
    }

    ~NotISR() override = default;

    bool hasNext() const override {
        return current_doc_id_ < doc_count_;
    }

    void moveNext() override {
        if (!hasNext()) {
            return;
        }

        current_doc_id_++;

        // Skip over any document IDs that exist in the underlying reader
        while (current_doc_id_ <= doc_count_ && 
               reader_->hasNext() && 
               reader_->currentDocID() <= current_doc_id_) {
            
            if (reader_->currentDocID() == current_doc_id_) {
                // This document ID exists in reader, so skip it
                current_doc_id_++;
                // Reset from the beginning if we need to
                if (current_doc_id_ <= doc_count_) {
                    reader_->seekToDocID(current_doc_id_);
                }
            } else if (reader_->currentDocID() < current_doc_id_) {
                // Advance the reader until we catch up
                reader_->moveNext();
            }
        }
    }

    data::docid_t currentDocID() const override {
        return current_doc_id_;
    }

    void seekToDocID(data::docid_t target_doc_id) override {
        if (target_doc_id < current_doc_id_) {
            // If seeking backwards, reset to the beginning
            reader_->seekToDocID(1);
            current_doc_id_ = 0;
        }
        
        // Skip to just before the target
        current_doc_id_ = target_doc_id - 1;
        
        // Then use moveNext to find the next valid document ID
        moveNext();
        
        // If we didn't land exactly on target_doc_id, the target must be
        // in the excluded set, so we don't need to do anything else
    }

    
private:
    std::unique_ptr<IndexStreamReader> reader_;
    data::docid_t current_doc_id_;
    size_t doc_count_;
};

}  // namespace mithril

#endif  // INDEX_NOTISR_H