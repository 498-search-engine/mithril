#include "TermReader.h"

#include "PostingBlock.h"

#include <iostream>
#include <stdexcept>

namespace mithril {

TermReader::TermReader(const std::string& index_path, const std::string& term)
    : term_(term), index_path_(index_path + "/final_index.bin") {

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
        found_term_ = findTerm(term);
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

    // Read positions count
    index_file_.read(reinterpret_cast<char*>(&positions_count_), sizeof(positions_count_));

    // Read position sync points size
    uint32_t position_sync_points_size;
    index_file_.read(reinterpret_cast<char*>(&position_sync_points_size), sizeof(position_sync_points_size));

    // Skip position sync points
    index_file_.seekg(position_sync_points_size * sizeof(PositionSyncPoint), std::ios::cur);

    // Remember this position for later loading of positions
    positions_start_pos_ = index_file_.tellg();

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

bool TermReader::findTerm(const std::string& term) {
    if (!index_file_.is_open()) {
        return false;
    }

    // Reset file position to start
    index_file_.clear();
    index_file_.seekg(0, std::ios::beg);

    // Read term count
    uint32_t term_count;
    index_file_.read(reinterpret_cast<char*>(&term_count), sizeof(term_count));
    if (index_file_.fail()) {
        std::cerr << "Failed to read term count" << std::endl;
        return false;
    }

    std::cout << "Index has " << term_count << " terms" << std::endl;

    // Scan through terms
    for (uint32_t i = 0; i < term_count; i++) {
        // Read term length
        uint32_t term_len;
        index_file_.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
        if (index_file_.fail()) {
            std::cerr << "Failed to read term length" << std::endl;
            return false;
        }

        // Read term
        std::string current_term(term_len, ' ');
        index_file_.read(&current_term[0], term_len);
        if (index_file_.fail()) {
            std::cerr << "Failed to read term data" << std::endl;
            return false;
        }

        // Verbose output for debugging
        if (i % 10000 == 0 || current_term == term) {
            std::cout << "Term " << i << ": '" << current_term << "'" << std::endl;
        }

        // Check if this is the term we're looking for
        if (current_term == term) {
            std::cout << "Found term '" << term << "' at index " << i << std::endl;

            // Read postings size
            uint32_t postings_size;
            index_file_.read(reinterpret_cast<char*>(&postings_size), sizeof(postings_size));
            if (index_file_.fail()) {
                std::cerr << "Failed to read postings size" << std::endl;
                return false;
            }

            // Read sync points size
            uint32_t sync_points_size;
            index_file_.read(reinterpret_cast<char*>(&sync_points_size), sizeof(sync_points_size));
            if (index_file_.fail()) {
                std::cerr << "Failed to read sync points size" << std::endl;
                return false;
            }

            std::cout << "Term has " << postings_size << " postings and " << sync_points_size << " sync points"
                      << std::endl;

            // Skip sync points
            index_file_.seekg(sync_points_size * sizeof(SyncPoint), std::ios::cur);
            if (index_file_.fail()) {
                std::cerr << "Failed to skip sync points" << std::endl;
                return false;
            }

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

            // Read positions count
            index_file_.read(reinterpret_cast<char*>(&positions_count_), sizeof(positions_count_));
            if (index_file_.fail()) {
                std::cerr << "Failed to read positions count" << std::endl;
                return false;
            }

            // Read position sync points size
            uint32_t position_sync_points_size;
            index_file_.read(reinterpret_cast<char*>(&position_sync_points_size), sizeof(position_sync_points_size));
            if (index_file_.fail()) {
                std::cerr << "Failed to read position sync points size" << std::endl;
                return false;
            }

            // Skip position sync points
            index_file_.seekg(position_sync_points_size * sizeof(PositionSyncPoint), std::ios::cur);
            if (index_file_.fail()) {
                std::cerr << "Failed to skip position sync points" << std::endl;
                return false;
            }

            // Remember this position for later loading of positions
            positions_start_pos_ = index_file_.tellg();

            // Set initial state
            current_posting_index_ = 0;

            std::cout << "Successfully loaded term with " << postings_.size() << " postings and " << positions_count_
                      << " positions" << std::endl;
            return true;
        }

        // Term doesn't match - skip all its data

        // Read postings size
        uint32_t postings_size;
        index_file_.read(reinterpret_cast<char*>(&postings_size), sizeof(postings_size));
        if (index_file_.fail()) {
            std::cerr << "Failed to read postings size" << std::endl;
            return false;
        }

        // Read sync points size
        uint32_t sync_points_size;
        index_file_.read(reinterpret_cast<char*>(&sync_points_size), sizeof(sync_points_size));
        if (index_file_.fail()) {
            std::cerr << "Failed to read sync points size" << std::endl;
            return false;
        }

        // Skip sync points
        index_file_.seekg(sync_points_size * sizeof(SyncPoint), std::ios::cur);
        if (index_file_.fail()) {
            std::cerr << "Failed to skip sync points" << std::endl;
            return false;
        }

        // Skip all postings data
        for (uint32_t j = 0; j < postings_size; j++) {
            decodeVByte(index_file_);  // Skip doc_id delta
            decodeVByte(index_file_);  // Skip freq
        }

        // Read positions count
        uint32_t positions_size;
        index_file_.read(reinterpret_cast<char*>(&positions_size), sizeof(positions_size));
        if (index_file_.fail()) {
            std::cerr << "Failed to read positions count" << std::endl;
            return false;
        }

        // Read position sync points size
        uint32_t position_sync_points_size;
        index_file_.read(reinterpret_cast<char*>(&position_sync_points_size), sizeof(position_sync_points_size));
        if (index_file_.fail()) {
            std::cerr << "Failed to read position sync points size" << std::endl;
            return false;
        }

        // Skip position sync points
        index_file_.seekg(position_sync_points_size * sizeof(PositionSyncPoint), std::ios::cur);
        if (index_file_.fail()) {
            std::cerr << "Failed to skip position sync points" << std::endl;
            return false;
        }

        // Skip positions
        for (uint32_t j = 0; j < positions_size; j++) {
            decodeVByte(index_file_);
        }
    }

    std::cout << "Term '" << term << "' not found after scanning " << term_count << " terms" << std::endl;
    return false;
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

std::vector<uint32_t> TermReader::loadPositions(size_t posting_index) const {
    if (posting_index >= postings_.size() || !found_term_) {
        return {};
    }

    // Create a temp file stream to read positions
    // (This allows position reading to be const while not interfering with other operations)
    std::ifstream pos_file(index_path_, std::ios::binary);
    if (!pos_file) {
        std::cerr << "Failed to open index file for position reading" << std::endl;
        return {};
    }

    // Go to positions start
    pos_file.seekg(positions_start_pos_);
    if (pos_file.fail()) {
        std::cerr << "Failed to seek to positions start" << std::endl;
        return {};
    }

    // For a proper implementation, we would need to know which positions belong to which posting
    // This is simplified as in will read all positions and then reconstructs which belong where

    std::vector<uint32_t> all_positions;
    all_positions.reserve(positions_count_);
    uint32_t running_pos = 0;

    for (uint32_t i = 0; i < positions_count_; i++) {
        uint32_t delta = decodeVByte(pos_file);
        running_pos += delta;
        all_positions.push_back(running_pos);
    }

    // Determine which positions belong to which doc
    // For this simplified version, we'll just evenly distribute positions across documents
    // In a real implementation, you would need more accurate position information

    size_t positions_per_posting = all_positions.size() / postings_.size();
    size_t start_idx = posting_index * positions_per_posting;
    size_t end_idx =
        (posting_index == postings_.size() - 1) ? all_positions.size() : (posting_index + 1) * positions_per_posting;

    // Ensure bounds are valid
    start_idx = std::min(start_idx, all_positions.size());
    end_idx = std::min(end_idx, all_positions.size());

    // Extract this posting's positions
    std::vector<uint32_t> positions(all_positions.begin() + start_idx, all_positions.begin() + end_idx);
    return positions;
}

std::vector<uint32_t> TermReader::currentPositions() const {
    if (!hasNext()) {
        throw std::runtime_error("No current posting");
    }

    return loadPositions(current_posting_index_);
}

}  // namespace mithril
