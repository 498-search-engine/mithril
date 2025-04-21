#include "TermQuote.h"

#include "IndexStreamReader.h"
#include "TermAND.h"
#include "TermReader.h"

#include <algorithm>

namespace mithril {

TermQuote::TermQuote(DocumentMapReader& doc_reader,
                     const std::string& index_path,
                     const std::vector<std::string>& quote,
                     const core::MemMapFile& index_file,
                     TermDictionary& term_dict,
                     PositionIndex& position_index)
    : doc_reader_(doc_reader),
      index_path_(index_path),
      quote_(quote),
      index_file_(index_file),
      term_dict_(term_dict),
      position_index_(position_index) {
    std::vector<std::unique_ptr<IndexStreamReader>> term_readers;
    for (const auto& term : quote) {
        auto ptr = new TermReader(index_path_, term, index_file_, term_dict_, position_index_);
        term_readers_.push_back(ptr);                                          // maintain for ourselves
        term_readers.emplace_back(reinterpret_cast<IndexStreamReader*>(ptr));  // what we pass to TermAND
    }
    stream_reader_ = std::make_unique<TermAND>(std::move(term_readers));

    // set into valid initial state
    findNextMatch();
    if (hasNext()) {
        current_doc_id_ = next_doc_id_;
        findNextMatch();
    }
}

bool TermQuote::hasNext() const {
    return !at_end_;
}

void TermQuote::moveNext() {
    if (hasNext()) {
        current_doc_id_ = next_doc_id_;
        findNextMatch();
    }
}

data::docid_t TermQuote::currentDocID() const {
    return current_doc_id_;
}

void TermQuote::seekToDocID(data::docid_t target_doc_id) {
    while (hasNext() && current_doc_id_ != target_doc_id)
        moveNext();
}

bool TermQuote::findNextMatch() {
    while (stream_reader_->hasNext()) {
        stream_reader_->moveNext();
        const auto& base_positions = term_readers_[0]->currentPositions();
        for (auto x : base_positions) {
            bool all_match = true;
            for (size_t i = 1; i < term_readers_.size(); ++i) {
                const auto& positions = term_readers_[i]->currentPositions();
                if (!std::binary_search(positions.begin(), positions.end(), x + i)) {
                    all_match = false;
                    break;
                }
            }
            if (all_match) {
                next_doc_id_ = stream_reader_->currentDocID();
                return true;
            }
        }
    }
    at_end_ = true;
    return false;
}


}  // namespace mithril
