#include "ISR.h"

#include <algorithm>
#include <cassert>

namespace mithril {

TermISR::TermISR(std::unique_ptr<BlockReader> reader, const std::string& term)
    : reader_(std::move(reader)), term_(term) {

    if (!reader_ || reader_->current_postings.empty() || !reader_->has_next) {
        is_at_end_ = true;
    } else {
        current_posting_ = &reader_->current_postings[0];
    }
}

uint32_t TermISR::getCurrentDocId() const {
    if (is_at_end_ || !current_posting_) {
        return UINT32_MAX;
    }
    return current_posting_->doc_id;
}

bool TermISR::next() {
    if (is_at_end_) {
        return false;
    }

    // Find the current position in the array
    size_t current_idx = current_posting_ - &reader_->current_postings[0];

    // Move to next position
    if (current_idx + 1 < reader_->current_postings.size()) {
        // Next posting in the same block
        current_posting_ = &reader_->current_postings[current_idx + 1];
        return true;
    } else {
        // Need to read the next block
        try {
            reader_->read_next();
            if (!reader_->has_next || reader_->current_postings.empty()) {
                is_at_end_ = true;
                return false;
            }
            current_posting_ = &reader_->current_postings[0];
            return true;
        } catch (const std::exception& e) {
            is_at_end_ = true;
            return false;
        }
    }
}

bool TermISR::nextDoc() {
    if (is_at_end_) {
        return false;
    }

    uint32_t current_doc = getCurrentDocId();

    // Keep advancing until we find a different document
    do {
        if (!next()) {
            return false;
        }
    } while (getCurrentDocId() == current_doc);

    return true;
}

bool TermISR::seek(uint32_t target_doc_id) {
    if (is_at_end_) {
        return false;
    }

    // Skip seeking if we're already past the target
    if (getCurrentDocId() >= target_doc_id) {
        return true;
    }

    // Try to use the find_posting method of BlockReader for efficient seeking
    current_posting_ = reader_->find_posting(target_doc_id);

    // If we didn't find an exact match, we need to find the next posting after target
    if (!current_posting_) {
        // Slow path: linear search
        while (getCurrentDocId() < target_doc_id) {
            if (!next()) {
                return false;
            }
        }
    }

    return true;
}

bool TermISR::atEnd() const {
    return is_at_end_;
}

uint32_t TermISR::getFrequency() const {
    if (is_at_end_ || !current_posting_) {
        return 0;
    }
    return current_posting_->freq;
}

std::vector<uint32_t> TermISR::getPositions() const {
    if (is_at_end_ || !current_posting_) {
        return {};
    }

    // Use the reader to get positions for the current posting
    return reader_->get_positions(current_posting_->doc_id);
}

/*********************************************************************************************************************/

AndISR::AndISR(std::vector<std::unique_ptr<ISR>> children) : children_(std::move(children)) {
    // Initialize by finding the first match
    if (!children_.empty()) {
        is_at_end_ = !findMatchingDocument();
        if (!is_at_end_) {
            current_doc_id_ = children_[0]->getCurrentDocId();
        }
    } else {
        is_at_end_ = true;
    }
}

uint32_t AndISR::getCurrentDocId() const {
    if (is_at_end_) {
        return UINT32_MAX;
    }
    return current_doc_id_;
}

bool AndISR::next() {
    // For AND, next() is same as nextDoc() since we match at document level
    return nextDoc();
}

bool AndISR::nextDoc() {
    if (is_at_end_ || children_.empty()) {
        return false;
    }

    // Advance the first child to the next document
    if (!children_[0]->nextDoc()) {
        is_at_end_ = true;
        return false;
    }

    // Try to find a new matching document
    if (!findMatchingDocument()) {
        is_at_end_ = true;
        return false;
    }

    current_doc_id_ = children_[0]->getCurrentDocId();
    return true;
}

bool AndISR::seek(uint32_t target_doc_id) {
    if (is_at_end_) {
        return false;
    }

    // Skip if we're already past the target
    if (getCurrentDocId() >= target_doc_id) {
        return true;
    }

    // Seek the first child (optimization: seek the rarest term first)
    if (!children_[0]->seek(target_doc_id)) {
        is_at_end_ = true;
        return false;
    }

    // Try to find a matching document
    if (!findMatchingDocument()) {
        is_at_end_ = true;
        return false;
    }

    current_doc_id_ = children_[0]->getCurrentDocId();
    return true;
}

bool AndISR::atEnd() const {
    return is_at_end_;
}

uint32_t AndISR::getFrequency() const {
    // For AND, return the minimum frequency of all children
    if (is_at_end_ || children_.empty()) {
        return 0;
    }

    uint32_t min_freq = children_[0]->getFrequency();
    for (size_t i = 1; i < children_.size(); i++) {
        min_freq = std::min(min_freq, children_[i]->getFrequency());
    }
    return min_freq;
}

bool AndISR::findMatchingDocument() {
    if (children_.empty()) {
        return false;
    }

    while (!children_[0]->atEnd()) {
        uint32_t candidate_doc_id = children_[0]->getCurrentDocId();
        bool all_match = true;

        // Check if all other children have this document
        for (size_t i = 1; i < children_.size(); i++) {
            // Seek this child to the candidate document
            if (!children_[i]->seek(candidate_doc_id)) {
                return false;  // This child is exhausted
            }

            // If exact match wasn't found, candidate fails
            if (children_[i]->getCurrentDocId() != candidate_doc_id) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            return true;  // All children match this document
        }

        // Find the maximum document ID among all children
        uint32_t max_doc_id = 0;
        for (const auto& child : children_) {
            max_doc_id = std::max(max_doc_id, child->getCurrentDocId());
        }

        // Advance first child to at least this document
        if (!children_[0]->seek(max_doc_id)) {
            return false;  // First child exhausted
        }
    }

    return false;  // First child exhausted
}

/*********************************************************************************************************************/

OrISR::OrISR(std::vector<std::unique_ptr<ISR>> children) : children_(std::move(children)) {
    // Find the child with the minimum document ID
    findMinimumISR();

    if (current_min_idx_ == -1) {
        is_at_end_ = true;
    } else {
        current_doc_id_ = children_[current_min_idx_]->getCurrentDocId();
    }
}

uint32_t OrISR::getCurrentDocId() const {
    if (is_at_end_) {
        return UINT32_MAX;
    }
    return current_doc_id_;
}

bool OrISR::next() {
    // For OR, next() is same as nextDoc() since we match at document level
    return nextDoc();
}

bool OrISR::nextDoc() {
    if (is_at_end_ || current_min_idx_ == -1) {
        return false;
    }

    // Remember current document
    uint32_t current_doc = current_doc_id_;

    // Advance all children at this document
    for (size_t i = 0; i < children_.size(); i++) {
        if (!children_[i]->atEnd() && children_[i]->getCurrentDocId() == current_doc) {
            children_[i]->nextDoc();
        }
    }

    // Find new minimum
    findMinimumISR();

    if (current_min_idx_ == -1) {
        is_at_end_ = true;
        return false;
    }

    current_doc_id_ = children_[current_min_idx_]->getCurrentDocId();
    return true;
}

bool OrISR::seek(uint32_t target_doc_id) {
    if (is_at_end_) {
        return false;
    }

    // Skip if we're already past the target
    if (getCurrentDocId() >= target_doc_id) {
        return true;
    }

    // Seek all children
    bool any_valid = false;
    for (auto& child : children_) {
        if (!child->atEnd() && child->seek(target_doc_id)) {
            any_valid = true;
        }
    }

    if (!any_valid) {
        is_at_end_ = true;
        return false;
    }

    // Find new minimum
    findMinimumISR();

    if (current_min_idx_ == -1) {
        is_at_end_ = true;
        return false;
    }

    current_doc_id_ = children_[current_min_idx_]->getCurrentDocId();
    return true;
}

bool OrISR::atEnd() const {
    return is_at_end_;
}

uint32_t OrISR::getFrequency() const {
    // For OR, return the sum of frequencies from all matching children
    if (is_at_end_) {
        return 0;
    }

    uint32_t total_freq = 0;
    for (const auto& child : children_) {
        if (!child->atEnd() && child->getCurrentDocId() == current_doc_id_) {
            total_freq += child->getFrequency();
        }
    }
    return total_freq;
}

void OrISR::findMinimumISR() {
    current_min_idx_ = -1;
    uint32_t min_doc_id = UINT32_MAX;

    for (size_t i = 0; i < children_.size(); i++) {
        if (!children_[i]->atEnd()) {
            uint32_t doc_id = children_[i]->getCurrentDocId();
            if (doc_id < min_doc_id) {
                min_doc_id = doc_id;
                current_min_idx_ = static_cast<int>(i);
            }
        }
    }
}

/*********************************************************************************************************************/

std::unique_ptr<ISR> createTermISR(const std::string& term, const std::string& index_path) {
    try {
        auto reader = std::make_unique<BlockReader>(index_path);
        return std::make_unique<TermISR>(std::move(reader), term);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

std::unique_ptr<ISR> createAndISR(std::vector<std::unique_ptr<ISR>> children) {
    return std::make_unique<AndISR>(std::move(children));
}

std::unique_ptr<ISR> createOrISR(std::vector<std::unique_ptr<ISR>> children) {
    return std::make_unique<OrISR>(std::move(children));
}

}  // namespace mithril