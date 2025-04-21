#include "TermPhrase.h"

#include "TermAND.h"
#include "TermDictionary.h"
#include "TermReader.h"
#include "core/mem_map_file.h"

#include <algorithm>

namespace mithril {

constexpr uint32_t kMaxSpanSize = 5;

TermPhrase::TermPhrase(const std::string& index_path,
                       const std::vector<std::string>& phrase,
                       const core::MemMapFile& index_file,
                       TermDictionary& term_dict,
                       PositionIndex& position_index)
    : index_path_(index_path),
      phrase_(phrase),
      index_file_(index_file),
      term_dict_(term_dict),
      position_index_(position_index) {
    std::vector<std::unique_ptr<IndexStreamReader>> term_readers;
    for (const auto& term : phrase) {
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

bool TermPhrase::hasNext() const {
    return !at_end_;
}

void TermPhrase::moveNext() {
    if (hasNext()) {
        current_doc_id_ = next_doc_id_;
        findNextMatch();
    }
}

data::docid_t TermPhrase::currentDocID() const {
    return current_doc_id_;
}

void TermPhrase::seekToDocID(data::docid_t target_doc_id) {
    while (hasNext() && current_doc_id_ != target_doc_id)
        moveNext();
}

bool TermPhrase::findNextMatch() {
    while (stream_reader_->hasNext()) {
        stream_reader_->moveNext();
        const auto& base_positions = term_readers_[0]->currentPositions();
        for (auto base_pos : base_positions) {
            bool all_match = true;
            auto last_pos = base_pos;

            for (size_t i = 1; i < term_readers_.size(); ++i) {
                const auto& positions = term_readers_[i]->currentPositions();

                auto it = std::lower_bound(positions.begin(), positions.end(), last_pos);
                if (it == positions.end() || *it - base_pos > kMaxSpanSize) {
                    all_match = false;
                    break;
                }

                last_pos = *it;
            }

            if (all_match && last_pos - base_pos <= kMaxSpanSize) {
                next_doc_id_ = stream_reader_->currentDocID();
                return true;
            }
        }
    }
    at_end_ = true;
    return false;
}

}  // namespace mithril
