#include "PositionIndex.h"

#include "TextPreprocessor.h"
#include "Utils.h"
#include "data/Writer.h"

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <queue>

namespace fs = std::filesystem;

namespace mithril {

// init static members
std::mutex PositionIndex::buffer_mutex_;
std::unordered_map<std::string, std::vector<PositionEntry>> PositionIndex::position_buffer_;
size_t PositionIndex::buffer_size_ = 0;
int PositionIndex::buffer_counter_ = 0;

template<std::integral T>
static inline T CopyFromBytes(const char* ptr) {
    T val;
    std::memcpy(&val, ptr, sizeof(val));
    return val;
}

PositionIndex::PositionIndex(const std::string& index_dir)
    : index_dir_(index_dir), data_file_(index_dir + "/positions.data") {
    // Load term -> position mapping
    loadPosDict();
}

PositionIndex::~PositionIndex() {}

// void PositionIndex::addPositions(const std::string& output_dir,
//                                  const std::string& term,
//                                  uint32_t doc_id,
//                                  const std::vector<uint32_t>& positions) {
//     if (positions.empty())
//         return;

//     std::lock_guard<std::mutex> lock(buffer_mutex_);

//     // Add the positions to the buffer
//     PositionEntry entry{doc_id, positions};
//     position_buffer_[term].push_back(std::move(entry));

//     // Update buffer size (approx)
//     buffer_size_ += sizeof(uint32_t) + positions.size() * sizeof(uint32_t);

//     // Flush if needed
//     if (buffer_size_ >= MAX_BUFFER_SIZE) {
//         flushBuffer(output_dir);
//     }
// }

void PositionIndex::addPositionsBatch(const std::string& output_dir,
                                      uint32_t doc_id,
                                      const std::vector<std::pair<std::string, FieldPositions>>& term_positions) {

    if (term_positions.empty())
        return;

    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t total_added_size = 0;

    for (const auto& [term, field_pos] : term_positions) {
        // Flatten positions while maintaining order
        std::vector<uint16_t> flat_positions;
        size_t total_positions = 0;
        for (const auto& pos_vec : field_pos.positions) {
            total_positions += pos_vec.size();
        }
        flat_positions.reserve(total_positions);

        for (const auto& pos_vec : field_pos.positions) {
            flat_positions.insert(flat_positions.end(), pos_vec.begin(), pos_vec.end());
        }

        if (!flat_positions.empty()) {
            PositionEntry entry{doc_id, field_pos.field_flags, std::move(flat_positions)};
            position_buffer_[term].push_back(std::move(entry));
            total_added_size += sizeof(uint32_t) + sizeof(uint8_t) + total_positions * sizeof(uint16_t);
        }
    }

    buffer_size_ += total_added_size;
    if (buffer_size_ >= MAX_BUFFER_SIZE) {
        flushBuffer(output_dir);
    }
}


bool PositionIndex::shouldStorePositions(const std::string& term, uint32_t freq, size_t total_terms) {
    // 1. Field-based filtering
    if (!term.empty()) {
        const char prefix = term[0];
        if (prefix == '#' || prefix == '%' || std::isupper(prefix)) {
            return true;  // Title/description/proper nouns
        }

        if (prefix == '@') {
            // Skip protocol-only URLs but keep paths
            return (term.find('/') != std::string::npos);
        }
    }

    // 2. Stopword filtering
    if (StopwordFilter::isStopword(term)) {
        return false;
    }

    // 3. freq-based filtering
    const bool is_common_term = freq > 3000;
    const bool is_doc_ubiquitous = (total_terms > 0) && (freq > total_terms / 8);
    if (is_common_term || is_doc_ubiquitous) {
        return false;
    }

    // 4. Minimum usefulness threshold
    return (freq > 2);
}

void PositionIndex::flushBuffer(const std::string& output_dir) {
    if (position_buffer_.empty())
        return;

    try {
        // Create positions dir if needed
        std::string pos_dir = output_dir + "/positions";
        if (!fs::exists(pos_dir)) {
            fs::create_directories(pos_dir);
        }

        std::string buffer_file = pos_dir + "/buffer_" + std::to_string(buffer_counter_++) + ".data";
        auto out = data::FileWriter{buffer_file.c_str()};

        // Write number of terms
        uint32_t term_count = position_buffer_.size();
        out.Write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));

        // Write each term's data
        for (const auto& [term, entries] : position_buffer_) {
            // Write term length and term
            uint32_t term_len = term.length();
            out.Write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
            out.Write(term.data(), term_len);

            // Write doc count
            uint32_t doc_count = entries.size();
            out.Write(reinterpret_cast<const char*>(&doc_count), sizeof(doc_count));

            // Write each doc's positions
            for (const auto& entry : entries) {
                // Write doc ID and position count
                out.Write(reinterpret_cast<const char*>(&entry.doc_id), sizeof(entry.doc_id));
                out.Write(reinterpret_cast<const char*>(&entry.field_flags), sizeof(entry.field_flags));

                uint32_t pos_count = entry.positions.size();
                out.Write(reinterpret_cast<const char*>(&pos_count), sizeof(pos_count));

                // Write delta-encoded positions
                uint16_t prev_pos = 0;
                for (uint16_t pos : entry.positions) {
                    uint16_t delta = pos - prev_pos;
                    VByteCodec::encode(delta, out);
                    prev_pos = pos;
                }
            }
        }

        position_buffer_.clear();
        buffer_size_ = 0;

    } catch (const std::exception& e) {
        spdlog::error("Error flushing position buffer: {}", e.what());
        position_buffer_.clear();
        buffer_size_ = 0;
    }
}

void PositionIndex::mergePositionBuffers(const std::string& output_dir) {
    std::string pos_dir = output_dir + "/positions";

    try {
        if (!position_buffer_.empty()) {
            flushBuffer(output_dir);
        }

        if (buffer_counter_ == 0) {
            spdlog::info("No position data to merge");
            return;
        }

        // Get all buffer files
        std::vector<std::string> buffer_files;
        for (int i = 0; i < buffer_counter_; i++) {
            std::string buffer_file = pos_dir + "/buffer_" + std::to_string(i) + ".data";
            if (fs::exists(buffer_file)) {
                buffer_files.push_back(buffer_file);
            }
        }

        spdlog::info("Merging {} position buffer files", buffer_files.size());

        // Open output files
        std::string data_file = output_dir + "/positions.data";
        data::FileWriter data_out(data_file.c_str());
        std::string posDict_file = output_dir + "/positions.dict";
        data::FileWriter posDict_out(posDict_file.c_str());

        // Structure to track a term being processed from a file
        struct TermInfo {
            std::string term;
            uint32_t doc_count;
            size_t stream_index;  // Index into streams vector

            bool operator<(const TermInfo& other) const {
                return term > other.term;  // Reverse for min-heap
            }
        };

        // Open input streams - keeping them in a vector for stable addresses
        std::vector<std::ifstream> streams;
        streams.reserve(buffer_files.size());

        // pqueue for merge
        std::priority_queue<TermInfo> queue;

        // init with first term from each file
        for (size_t i = 0; i < buffer_files.size(); i++) {
            streams.emplace_back(buffer_files[i], std::ios::binary);
            std::ifstream& stream = streams.back();

            if (!stream) {
                spdlog::warn("Failed to open buffer file: {}", buffer_files[i]);
                continue;
            }

            // Read term count
            uint32_t term_count;
            if (!stream.read(reinterpret_cast<char*>(&term_count), sizeof(term_count))) {
                spdlog::warn("Failed to read term count from file: {}", buffer_files[i]);
                continue;
            }

            if (term_count > 0) {
                // Read first term
                uint32_t term_len;
                if (!stream.read(reinterpret_cast<char*>(&term_len), sizeof(term_len))) {
                    spdlog::warn("Failed to read term length from file: {}", buffer_files[i]);
                    continue;
                }

                std::string term(term_len, ' ');
                if (!stream.read(&term[0], term_len)) {
                    spdlog::warn("Failed to read term string from file: {}", buffer_files[i]);
                    continue;
                }

                // Read doc count
                uint32_t doc_count;
                if (!stream.read(reinterpret_cast<char*>(&doc_count), sizeof(doc_count))) {
                    spdlog::warn("Failed to read doc count from file: {}", buffer_files[i]);
                    continue;
                }

                // Add to priority queue
                queue.push({term, doc_count, i});
            }
        }

        // Write placeholder for term count (will update later)
        uint32_t total_terms = 0;
        posDict_out.Write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

        // Process all terms
        std::string current_term;
        TermPositions current_positions;

        while (!queue.empty()) {
            // Get next term from queue
            TermInfo info = queue.top();
            queue.pop();

            // Safety check for stream index
            if (info.stream_index >= streams.size() || !streams[info.stream_index]) {
                spdlog::error("Invalid stream index or bad stream: {}", info.stream_index);
                continue;
            }

            std::ifstream& stream = streams[info.stream_index];

            // If new term, write previous term data
            if (current_term != info.term) {
                if (!current_term.empty()) {
                    // Write previous term
                    if (!writeTerm(current_term, current_positions, data_out, posDict_out)) {
                        throw std::runtime_error("Failed to write term: " + current_term);
                    }
                    total_terms++;
                }

                // Start new term
                current_term = info.term;
                current_positions.clear();
            }

            // Read all docs for this term
            for (uint32_t i = 0; i < info.doc_count && stream.good(); i++) {
                // Read doc ID
                uint32_t doc_id;
                if (!stream.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id))) {
                    spdlog::error("Failed to read doc ID for term: {}", info.term);
                    break;
                }

                // Read field flags
                uint8_t field_flags;
                if (!stream.read(reinterpret_cast<char*>(&field_flags), sizeof(field_flags))) {
                    spdlog::error("Failed to read field flags for doc: {}", doc_id);
                    break;
                }

                // Read position count
                uint32_t pos_count;
                if (!stream.read(reinterpret_cast<char*>(&pos_count), sizeof(pos_count))) {
                    spdlog::error("Failed to read position count for doc: {}", doc_id);
                    break;
                }

                // Read positions
                std::vector<uint16_t> positions;
                positions.reserve(pos_count);

                uint16_t prev_pos = 0;
                bool read_success = true;

                for (uint32_t j = 0; j < pos_count; j++) {
                    try {
                        uint16_t delta = VByteCodec::decode(stream);
                        prev_pos += delta;
                        positions.push_back(prev_pos);
                    } catch (const std::exception& e) {
                        spdlog::error("Error decoding position: {}", e.what());
                        read_success = false;
                        break;
                    }
                }

                if (read_success) {
                    current_positions.emplace_back(doc_id, std::make_pair(field_flags, std::move(positions)));
                }
            }

            // Try to read next term from this file
            if (stream.good()) {
                // Read next term length
                uint32_t term_len;
                if (!stream.read(reinterpret_cast<char*>(&term_len), sizeof(term_len))) {
                    // End of file or error, continue to next stream
                    continue;
                }

                // Read term
                std::string term(term_len, ' ');
                if (!stream.read(&term[0], term_len)) {
                    continue;
                }

                // Read doc count
                uint32_t doc_count;
                if (!stream.read(reinterpret_cast<char*>(&doc_count), sizeof(doc_count))) {
                    continue;
                }

                // Add to queue
                queue.push({term, doc_count, info.stream_index});
            }
        }

        // Write final term if any
        if (!current_term.empty()) {
            if (!writeTerm(current_term, current_positions, data_out, posDict_out)) {
                throw std::runtime_error("Failed to write final term: " + current_term);
            }
            total_terms++;
        }

        // Update term count
        posDict_out.Fseek(0);
        posDict_out.Write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

        // Close files
        data_out.Close();
        posDict_out.Close();

        // Clean up
        for (const auto& buffer_file : buffer_files) {
            fs::remove(buffer_file);
        }
        fs::remove_all(pos_dir);

        spdlog::info("Position index merge complete. Total terms: {}", total_terms);
    } catch (const std::exception& e) {
        spdlog::error("Error merging position buffers: {}", e.what());
    }
}

bool PositionIndex::writeTerm(const std::string& term,
                              const TermPositions& docs_positions,
                              data::FileWriter& data_out,
                              data::FileWriter& posDict_out) {
    try {
        // Sort docs by ID
        TermPositions sorted_docs = docs_positions;
        std::sort(
            sorted_docs.begin(), sorted_docs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        // Write term to dictionary
        uint32_t term_len = term.length();

        posDict_out.Write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        posDict_out.Write(term.data(), term_len);

        // Prepare metadata
        PositionMetadata metadata;
        metadata.data_offset = static_cast<uint64_t>(data_out.Ftell());
        metadata.doc_count = sorted_docs.size();
        metadata.total_positions = 0;

        for (const auto& [doc_id, data] : sorted_docs) {
            metadata.total_positions += data.second.size();
        }

        posDict_out.Write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));

        // Write docs
        for (const auto& [doc_id, data] : sorted_docs) {
            const auto& [field_flags, positions] = data;


            data_out.Write(reinterpret_cast<const char*>(&doc_id), sizeof(doc_id));

            data_out.Write(reinterpret_cast<const char*>(&field_flags), sizeof(field_flags));

            uint32_t pos_count = positions.size();
            data_out.Write(reinterpret_cast<const char*>(&pos_count), sizeof(pos_count));

            uint16_t prev_pos = 0;
            for (uint16_t pos : positions) {
                uint16_t delta = pos - prev_pos;
                VByteCodec::encode(delta, data_out);
                prev_pos = pos;
            }
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error writing term {}: {}", term, e.what());
        return false;
    }
}

void PositionIndex::finalizeIndex(const std::string& output_dir) {
    try {
        // Merge buffer files
        mergePositionBuffers(output_dir);

        // Reset state for next index
        buffer_counter_ = 0;
    } catch (const std::exception& e) {
        spdlog::error("Error finalizing position index: {}", e.what());
    }
}

bool PositionIndex::loadPosDict() {
    std::string posDict_file = index_dir_ + "/positions.dict";

    try {
        std::ifstream in(posDict_file, std::ios::binary);
        if (!in) {
            spdlog::warn("No position dict file found: {}", posDict_file);
            return false;
        }

        // Read term count
        uint32_t term_count;
        in.read(reinterpret_cast<char*>(&term_count), sizeof(term_count));

        // Read term data
        for (uint32_t i = 0; i < term_count && in.good(); i++) {
            // Read term
            uint32_t term_len;
            in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
            std::string term(term_len, ' ');
            in.read(&term[0], term_len);

            // Read metadata
            PositionMetadata metadata;
            in.read(reinterpret_cast<char*>(&metadata.data_offset), sizeof(metadata.data_offset));
            in.read(reinterpret_cast<char*>(&metadata.doc_count), sizeof(metadata.doc_count));
            in.read(reinterpret_cast<char*>(&metadata.total_positions), sizeof(metadata.total_positions));

            // Add to dict
            posDict_[term] = metadata;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error loading position dict: {}", e.what());
        return false;
    }
}

bool PositionIndex::hasPositions(const std::string& term, uint32_t doc_id) const {
    auto it = posDict_.find(term);
    if (it == posDict_.end()) {
        return false;
    }

    try {
        // Get positions
        std::vector<uint16_t> positions = getPositions(term, doc_id);
        return !positions.empty();
    } catch (const std::exception& e) {
        spdlog::error("Error checking positions: {}", e.what());
        return false;
    }
}

std::vector<uint16_t> PositionIndex::getPositions(const std::string& term, uint32_t doc_id) const {
    auto it = posDict_.find(term);
    if (it == posDict_.end()) {
        return {};
    }

    auto data_ptr = data_file_.data();
    const auto data_end = data_ptr + data_file_.size();

    try {  // TODO: remove exceptions
        const PositionMetadata& metadata = it->second;
        data_ptr += metadata.data_offset;

        for (uint32_t i = 0; i < metadata.doc_count; i++) {
            const uint32_t curr_doc_id = CopyFromBytes<uint32_t>(data_ptr);
            data_ptr += sizeof(curr_doc_id);

            const uint8_t field_flags = CopyFromBytes<uint32_t>(data_ptr);
            data_ptr += sizeof(field_flags);

            const uint32_t pos_count = CopyFromBytes<uint32_t>(data_ptr);
            data_ptr += sizeof(pos_count);

            if (curr_doc_id == doc_id) {
                std::vector<uint16_t> positions;
                positions.reserve(pos_count);

                uint16_t prev_pos = 0;
                for (uint32_t j = 0; j < pos_count; j++) {
                    uint16_t delta = decodeVByte(data_ptr);
                    prev_pos += delta;
                    positions.push_back(prev_pos);
                }

                return positions;
            }

            // Otherwise, skip positions for this doc
            for (uint32_t j = 0; j < pos_count; j++) {
                (void)decodeVByte(data_ptr);
            }
        }

        return {};
    } catch (const std::exception& e) {
        spdlog::error("Error getting positions: {}", e.what());
        return {};
    }
}

uint8_t PositionIndex::getFieldFlags(const std::string& term, uint32_t doc_id) const {
    auto it = posDict_.find(term);
    if (it == posDict_.end()) {
        return {};
    }

    auto data_ptr = data_file_.data();
    const auto data_end = data_ptr + data_file_.size();

    try {  // TODO: remove exceptions
        const PositionMetadata& metadata = it->second;
        data_ptr += metadata.data_offset;

        for (uint32_t i = 0; i < metadata.doc_count; i++) {
            const uint32_t curr_doc_id = CopyFromBytes<uint32_t>(data_ptr);
            data_ptr += sizeof(curr_doc_id);

            const uint8_t field_flags = CopyFromBytes<uint32_t>(data_ptr);
            data_ptr += sizeof(field_flags);

            const uint32_t pos_count = CopyFromBytes<uint32_t>(data_ptr);
            data_ptr += sizeof(pos_count);

            if (curr_doc_id == doc_id) {
                return field_flags;
            }

            // Otherwise, skip positions for this doc
            for (uint32_t j = 0; j < pos_count; j++) {
                (void)decodeVByte(data_ptr);
            }
        }

        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Error getting field flags: {}", e.what());
        return 0;
    }
}

bool PositionIndex::checkPhrase(const std::string& term1,
                                const std::string& term2,
                                uint32_t doc_id,
                                int distance) const {
    try {
        // Get positions for both terms
        std::vector<uint16_t> positions1 = getPositions(term1, doc_id);
        std::vector<uint16_t> positions2 = getPositions(term2, doc_id);

        if (positions1.empty() || positions2.empty()) {
            return false;
        }

        // Check if any position in term1 is followed by a position in term2 at the specified distance
        for (uint32_t pos1 : positions1) {
            uint32_t target = pos1 + distance;

            // Binary search for target position in positions2
            auto it = std::lower_bound(positions2.begin(), positions2.end(), target);
            if (it != positions2.end() && *it == target) {
                return true;  // Found a match
            }
        }

        return false;  // No phrase match found
    } catch (const std::exception& e) {
        spdlog::error("Error checking phrase: {}", e.what());
        return false;
    }
}

uint32_t PositionIndex::decodeVByte(const char*& ptr) const {
    uint32_t result = 0, shift = 0;
    uint8_t byte;

    const auto end = data_file_.data() + data_file_.size();
    while (ptr < end) [[likely]] {
        uint8_t byte = *reinterpret_cast<const uint8_t*>(ptr++);
        result |= (uint32_t)(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            break;
        shift += 7;
    }

    return result;
}

}  // namespace mithril
