#include "TermReader.h"

#include "PostingBlock.h"

#include <iostream>
#include <stdexcept>

namespace mithril {

TermReader::TermReader(const std::string& index_path, const std::string& term)
    : term_(term), index_path_(index_path + "/final_index.data") {

    // Open the index file
    index_file_.open(index_path_, std::ios::binary);
    if (!index_file_) {
        throw std::runtime_error("Failed to open index file: " + index_path_);
    }

    // Create dictionary once
    TermDictionary dictionary(index_path);

    if (dictionary.is_loaded()) {
        found_term_ = findTermWithDict(term, dictionary);
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

    // calc abs file position (skip past term count)
    std::streampos term_start_pos = sizeof(uint32_t);  // Skip term count
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

    // Read sync points size
    uint32_t sync_points_size;
    index_file_.read(reinterpret_cast<char*>(&sync_points_size), sizeof(sync_points_size));

    // Skip sync points
    index_file_.seekg(sync_points_size * sizeof(SyncPoint), std::ios::cur);

    // Read postings
    postings_.clear();
    postings_.reserve(postings_size);
    uint32_t last_doc_id = 0;
    for (uint32_t j = 0; j < postings_size; j++) {
        uint32_t doc_id_delta = decodeVByte(index_file_);
        uint32_t freq = decodeVByte(index_file_);
        uint32_t doc_id = last_doc_id + doc_id_delta;
        last_doc_id = doc_id;
        postings_.emplace_back(doc_id, freq);
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

    // If we're already past the target, we need to reset
    if (current_posting_index_ >= postings_.size() || postings_[current_posting_index_].first > target_doc_id) {
        current_posting_index_ = 0;
    }

    // Scan forward to find target or next greater
    while (current_posting_index_ < postings_.size() && postings_[current_posting_index_].first < target_doc_id) {
        current_posting_index_++;
    }

    // Check if we reached the end
    if (current_posting_index_ >= postings_.size()) {
        at_end_ = true;
    }
}

bool TermReader::hasPositions() const {
    if (!found_term_ || at_end_) {
        return false;
    }

    if (!position_index_) {
        // Extract just the dir part of the index path
        std::string index_dir = index_path_;
        size_t last_slash = index_dir.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            index_dir = index_dir.substr(0, last_slash);
        }

        position_index_ = std::make_shared<PositionIndex>(index_dir);
    }

    return position_index_->hasPositions(term_, currentDocID());
}

std::vector<uint32_t> TermReader::currentPositions() const {
    if (!found_term_ || at_end_) {
        return {};
    }
    if (!position_index_) {
        position_index_ = std::make_shared<PositionIndex>(index_path_);
    }
    return position_index_->getPositions(term_, currentDocID());
}

}  // namespace mithril
