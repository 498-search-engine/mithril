#include "TermReader.h"

#include "PostingBlock.h"
#include "core/mem_map_file.h"

#include <concepts>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace mithril {

template<std::integral T>
static inline T CopyFromBytes(const char* ptr) {
    T val;
    std::memcpy(&val, ptr, sizeof(val));
    return val;
}

TermReader::TermReader(const std::string& index_path,
                       const std::string& term,
                       const core::MemMapFile& index_file,
                       TermDictionary& term_dict,
                       PositionIndex& position_index)
    : term_dict_(term_dict),
      term_(term),
      index_path_(index_path + "/final_index.data"),
      index_dir_(index_path),
      index_file_(index_file),
      position_index_(position_index) {

    if (term_dict_.is_loaded()) {
        found_term_ = findTermWithDict(term, term_dict_);
    } else {
        // Fall back to sequential scan
        // found_term_ = findTerm(term);
        throw std::runtime_error("Failed to load term dictionary");
    }

    if (!found_term_) {
        at_end_ = true;
    }
}

TermReader::~TermReader() {}

bool TermReader::findTermWithDict(const std::string& term, const TermDictionary& dictionary) {
    auto entry_opt = dictionary.lookup(term);
    if (!entry_opt) {
        return false;
    }

    // Calculate abs file position (skip past 32bit term count)
    const auto list_offset = sizeof(uint32_t) + entry_opt->index_offset;

    // Seek directly to the term position
    auto file_ptr = index_file_.data() + list_offset;

    // Read term length and verify
    const uint32_t term_len = CopyFromBytes<uint32_t>(file_ptr);
    file_ptr += sizeof(uint32_t);
    if (term_len != term.length()) {
        std::cerr << "Dictionary offset error: term length mismatch" << std::endl;
        return false;
    }

    // Skip term content since we already know it matches
    file_ptr += term_len;

    // Read postings size
    const uint32_t postings_size = CopyFromBytes<uint32_t>(file_ptr);
    file_ptr += sizeof(postings_size);

    // Read sync points
    const uint32_t sync_points_size = CopyFromBytes<uint32_t>(file_ptr);
    file_ptr += sizeof(sync_points_size);

    // Load sync points
    sync_points_.resize(sync_points_size);
    if (sync_points_size > 0) {
        std::memcpy(sync_points_.data(), file_ptr, sync_points_size * sizeof(SyncPoint));
        file_ptr += sync_points_size * sizeof(SyncPoint);
    }

    // Read postings
    postings_.clear();
    postings_.reserve(postings_size);

    // First read all doc ID deltas and calculate actual doc IDs
    std::vector<uint32_t> doc_ids(postings_size);
    uint32_t last_doc_id = 0;
    for (uint32_t j = 0; j < postings_size; ++j) {
        const uint32_t doc_id_delta = decodeVByte(file_ptr);
        last_doc_id += doc_id_delta;
        doc_ids[j] = last_doc_id;
    }

    // Then read all frequencies
    std::vector<uint32_t> freqs(postings_size);
    for (uint32_t j = 0; j < postings_size; j++) {
        freqs[j] = decodeVByte(file_ptr);
    }

    // Combine doc IDs and frequencies into postings
    for (uint32_t j = 0; j < postings_size; j++) {
        postings_.emplace_back(doc_ids[j], freqs[j]);
    }

    // Set initial state
    current_posting_index_ = 0;
    std::cout << "Successfully loaded term '" << term << "' using dictionary lookup" << std::endl;
    return true;
}

uint32_t TermReader::decodeVByte(const char*& ptr) {
    uint32_t result = 0, shift = 0;
    uint8_t byte;

    const auto end = index_file_.data() + index_file_.size();
    while (ptr < end) [[likely]] {
        uint8_t byte = *reinterpret_cast<const uint8_t*>(ptr++);
        result |= (uint32_t)(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            break;
        shift += 7;
    }

    return result;
}

bool TermReader::hasNext() const {
    if (!found_term_ || at_end_) {
        return false;
    }

    return current_posting_index_ < postings_.size();
}

void TermReader::moveNext() {
    if (!hasNext()) {
        at_end_ = true;
        return;
    }

    current_posting_index_++;
}

data::docid_t TermReader::currentDocID() const {
    if (!hasNext()) {
        throw std::runtime_error("No current posting");
    }

    return postings_[current_posting_index_].first;
}

uint32_t TermReader::currentFrequency() const {
    if (!hasNext()) {
        throw std::runtime_error("No current posting");
    }

    return postings_[current_posting_index_].second;
}

void TermReader::seekToDocID(data::docid_t target_doc_id) {
    if (!found_term_ || at_end_) {
        return;
    }

    // 1. Check if we're already at or past the target
    if (current_posting_index_ < postings_.size() && postings_[current_posting_index_].first >= target_doc_id) {
        return;  // Already at or past target
    }

    // 2. Check if we need to skip to the end
    if (target_doc_id > postings_.back().first) {
        current_posting_index_ = postings_.size();
        at_end_ = true;
        return;
    }

    // 3. Binary search using sync points if available
    if (!sync_points_.empty()) {
        // Find largest sync point with doc_id <= target_doc_id
        size_t left = 0;
        size_t right = sync_points_.size() - 1;
        size_t best_idx = 0;

        while (left <= right) {
            size_t mid = left + (right - left) / 2;

            if (sync_points_[mid].doc_id <= target_doc_id) {
                best_idx = mid;
                left = mid + 1;
            } else {
                if (mid == 0)
                    break;
                right = mid - 1;
            }
        }

        // Start from the closest sync point
        current_posting_index_ = sync_points_[best_idx].plist_offset;
    }

    // 4. Linear scan from current position to target
    while (current_posting_index_ < postings_.size() && postings_[current_posting_index_].first < target_doc_id) {
        current_posting_index_++;
    }

    // 5. Check if we've reached the end
    if (current_posting_index_ >= postings_.size()) {
        at_end_ = true;
    }
}

bool TermReader::hasPositions() const {
    if (!found_term_ || at_end_) {
        return false;
    }

    return position_index_.hasPositions(term_, currentDocID());
}

std::vector<uint16_t> TermReader::currentPositions() const {
    if (!found_term_ || at_end_) {
        return {};
    }

    return position_index_.getPositions(term_, currentDocID());
}

double TermReader::getAverageFrequency() const {
    if (avg_frequency_computed_) {
        return avg_frequency_;
    }

    double total_freq = 0.0;
    for (const auto& posting : postings_) {
        total_freq += posting.second;
    }

    if (!postings_.empty()) {
        avg_frequency_ = total_freq / postings_.size();
    } else {
        avg_frequency_ = 0.0;
    }

    avg_frequency_computed_ = true;
    return avg_frequency_;
}

}  // namespace mithril
