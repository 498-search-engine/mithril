#ifndef INDEX_NOTISR_H
#define INDEX_NOTISR_H

#include "DocumentMapReader.h"
#include "IndexStreamReader.h"
#include "PositionIndex.h"
#include "TermDictionary.h"

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mithril {

class NotISR : public IndexStreamReader {
public:
    explicit NotISR(std::unique_ptr<IndexStreamReader> reader_in, size_t document_count_in)
        : reader_(std::move(reader_in))
        , current_doc_id_(0)
        , doc_at_end_(false)
        , max_doc_id_(document_count_in)
    {
    }

    ~NotISR() override {
        return;
    }


    // ISR

    size_t get_boundary_doc_id(){
        if (doc_at_end_){
            return max_doc_id_; 
        }
        return reader_->currentDocID();
    }

    bool atEnd() const { 
        return doc_at_end_;
    }

    bool hasNext() const override{
        return !atEnd();
    }

    void move_reader_next() {
        if (!reader_->hasNext()) {
            return;
        }
        reader_->moveNext();
    }

    void moveNext() override {
        if (atEnd()) {
            return;
        }

        while (current_doc_id_ < max_doc_id_) {
            if (current_doc_id_ < get_boundary_doc_id()) {
                current_doc_id_ += 1;
            } else {
                move_reader_next();
            }
        }

        if (current_doc_id_ >= max_doc_id_) {
            doc_at_end_ = true;
        }
    }

    data::docid_t currentDocID() const override {
        if (current_doc_id_ > max_doc_id_){
            throw std::runtime_error("Current document ID exceeds maximum document ID.");
        }
        return current_doc_id_; 
    }


    void seekToDocID(data::docid_t target_doc_id) override {
        if (target_doc_id >= max_doc_id_) {
            current_doc_id_ = max_doc_id_;
            doc_at_end_ = true;
            return;
        }

        if (target_doc_id < current_doc_id_) {
            // Reset the reader to the beginning
            reader_->seekToDocID(0);
            current_doc_id_ = 0;
            doc_at_end_ = false;
        }

        while (current_doc_id_ < target_doc_id && !doc_at_end_) {
            moveNext();
        }
    }

    // term specific funcs
    // uint32_t currentFrequency() const;
    // std::string getTerm() const { return term_; }
    // uint32_t getDocumentCount() const { return postings_.size(); }

    // postion specific funcs
    // bool hasPositions() const;
    // std::vector<uint32_t> currentPositions() const;

private:

    std::unique_ptr<IndexStreamReader> reader_;
    data::docid_t current_doc_id_;
    data::docid_t max_doc_id_;
    bool doc_at_end_;

};

}  // namespace mithril

#endif  // INDEX_NOTISR_H
