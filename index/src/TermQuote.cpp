#include "TermQuote.h"

#include "IndexStreamReader.h"
#include "TermAND.h"
#include "TermReader.h"

#include <algorithm>

namespace mithril {

TermQuote::TermQuote(const std::string& index_path,
                     const std::vector<std::string>& quote,
                     const core::MemMapFile& index_file,
                     TermDictionary& term_dict,
                     PositionIndex& position_index)
    : index_path_(index_path),
      quote_(quote),
      index_file_(index_file),
      term_dict_(term_dict),
      position_index_(position_index),
      current_doc_id_(0),
      next_doc_id_(0),
      at_end_(false) {
    if (quote.empty()) {
        at_end_ = true;
        return;
    }

    std::vector<std::unique_ptr<IndexStreamReader>> term_readers;
    for (const auto& term : quote) {
        auto ptr = new TermReader(index_path_, term, index_file_, term_dict_, position_index_);
        term_readers_.push_back(ptr);                                          // maintain for ourselves
        term_readers.emplace_back(reinterpret_cast<IndexStreamReader*>(ptr));  // what we pass to TermAND
    }
    stream_reader_ = std::make_unique<TermAND>(std::move(term_readers));

    // Initialize the iterator state
    if (stream_reader_->hasNext()) {
        stream_reader_->moveNext();
        if (findNextMatch()) {
            current_doc_id_ = next_doc_id_;
        } else {
            at_end_ = true;
        }
    } else {
        at_end_ = true;
    }
}

bool TermQuote::hasNext() const {
    return !at_end_;
}

void TermQuote::moveNext() {
    if (hasNext()) {
        current_doc_id_ = next_doc_id_;
        if (!findNextMatch()) {
            at_end_ = true;
        }
    }
}

data::docid_t TermQuote::currentDocID() const {
    return current_doc_id_;
}

void TermQuote::seekToDocID(data::docid_t target_doc_id) {
    if (current_doc_id_ >= target_doc_id) {
        return;  // Already at or beyond target
    }
    
    stream_reader_->seekToDocID(target_doc_id);
    if (!stream_reader_->hasNext() || stream_reader_->currentDocID() != target_doc_id) {
        at_end_ = true;
        return;
    }
    
    if (!findNextMatch()) {
        at_end_ = true;
    } else {
        current_doc_id_ = next_doc_id_;
    }
}

bool TermQuote::findNextMatch() {
    // If we're already at the end or stream reader has no more items, we're done
    if (at_end_ || !stream_reader_->hasNext()) {
        at_end_ = true;
        return false;
    }
    
    // Current document should already be positioned by the stream reader
    const auto current_stream_doc = stream_reader_->currentDocID();
    
    do {
        // Get positions for the first term
        const auto& base_positions = term_readers_[0]->currentPositions();
        
        // For each position of the first term, check if we have a matching sequence
        for (auto start_pos : base_positions) {
            bool phrase_match = true;
            
            // Check each subsequent term
            for (size_t i = 1; i < term_readers_.size(); ++i) {
                const auto& positions = term_readers_[i]->currentPositions();
                
                // The next term should be at exactly position start_pos + i
                if (!std::binary_search(positions.begin(), positions.end(), start_pos + i)) {
                    phrase_match = false;
                    break;
                }
            }
            
            if (phrase_match) {
                // We found a phrase match
                next_doc_id_ = current_stream_doc;
                return true;
            }
        }
        
        // No match in this document, move to next document
        if (stream_reader_->hasNext()) {
            stream_reader_->moveNext();
        } else {
            at_end_ = true;
            return false;
        }
        
    } while (stream_reader_->hasNext());
    
    at_end_ = true;
    return false;
}

TermQuote::~TermQuote() {

}

}  // namespace mithril
