#include "TermReader.h"

#include "PostingBlock.h"

#include <iostream>
#include <stdexcept>

namespace mithril {

TermReader::TermReader(const std::string& index_path, const std::string& term, TermDictionary& term_dict)
    : term_dict_(term_dict), term_(term), index_path_(index_path + "/final_index.data"), index_dir_(index_path) {

    // Open the index file
    index_file_.open(index_path_, std::ios::binary);
    if (!index_file_) {
        throw std::runtime_error("Failed to open index file: " + index_path_);
    }

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

TermReader::~TermReader() {
    if (index_file_.is_open()) {
        index_file_.close();
    }
}

bool TermReader::findTermWithDict(const std::string& term, const TermDictionary& dictionary) {
    auto entry_opt = dictionary.lookup(term);
    if (!entry_opt) {
        return false;
    }

    // Calculate abs file position (skip past term count)
    std::streampos term_start_pos = sizeof(uint32_t);
    std::streampos absolute_pos = term_start_pos + static_cast<std::streampos>(entry_opt->index_offset);

    // Seek directly to the term position
    index_file_.seekg(absolute_pos);

    // Read term length and verify
    uint32_t term_len;
    index_file_.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
    if (term_len != term.length()) {
        std::cerr << "Dictionary offset error: term length mismatch" << std::endl;
        return false;
    }

    // Skip term content since we already know it matches
    index_file_.seekg(term_len, std::ios::cur);

    // Read postings size
    uint32_t postings_size;
    index_file_.read(reinterpret_cast<char*>(&postings_size), sizeof(postings_size));

    // Read sync points
    uint32_t sync_points_size;
    index_file_.read(reinterpret_cast<char*>(&sync_points_size), sizeof(sync_points_size));

    // Load sync points
    sync_points_.resize(sync_points_size);
    if (sync_points_size > 0) {
        index_file_.read(reinterpret_cast<char*>(sync_points_.data()), sync_points_size * sizeof(SyncPoint));
    }

    // Read postings
    postings_.clear();
    postings_.reserve(postings_size);

    // First read all doc ID deltas and calculate actual doc IDs
    std::vector<uint32_t> doc_ids(postings_size);
    uint32_t last_doc_id = 0;
    for (uint32_t j = 0; j < postings_size; j++) {
        uint32_t doc_id_delta = decodeVByte(index_file_);
        uint32_t doc_id = last_doc_id + doc_id_delta;
        doc_ids[j] = doc_id;
        last_doc_id = doc_id;
    }

    // Then read all frequencies
    std::vector<uint32_t> freqs(postings_size);
    for (uint32_t j = 0; j < postings_size; j++) {
        freqs[j] = decodeVByte(index_file_);
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

uint32_t TermReader::decodeVByte(std::istream& in) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;

    do {
        byte = in.get();
        if (in.fail()) {
            throw std::runtime_error("Failed to read VByte encoded value");
        }
        result |= (byte & 127) << shift;
        shift += 7;
    } while (byte & 128);

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

    if (!position_index_) {
        // // Extract just the dir part of the index path
        // std::string index_dir = index_path_;
        // size_t last_slash = index_dir.find_last_of("/\\");
        // if (last_slash != std::string::npos) {
        //     index_dir = index_dir.substr(0, last_slash);
        // }

        position_index_ = std::make_shared<PositionIndex>(index_dir_);
    }

    return position_index_->hasPositions(term_, currentDocID());
}

std::vector<uint16_t> TermReader::currentPositions() const {
    if (!found_term_ || at_end_) {
        return {};
    }
    if (!position_index_) {
        position_index_ = std::make_shared<PositionIndex>(index_dir_);
    }
    return position_index_->getPositions(term_, currentDocID());
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
