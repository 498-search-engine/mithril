#include "PositionIndex.h"

#include "TextPreprocessor.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace mithril {

// init static members
std::mutex PositionIndex::buffer_mutex_;
std::unordered_map<std::string, std::vector<PositionEntry>> PositionIndex::position_buffer_;
size_t PositionIndex::buffer_size_ = 0;
int PositionIndex::buffer_counter_ = 0;

PositionIndex::PositionIndex(const std::string& index_dir) : index_dir_(index_dir) {
    // Load term -> position mapping
    loadPosDict();
    std::string data_file = index_dir + "/positions.data";
    data_file_.open(data_file, std::ios::binary);
}

PositionIndex::~PositionIndex() {
    if (data_file_.is_open()) {
        data_file_.close();
    }
}

void PositionIndex::addPositions(const std::string& output_dir,
                                 const std::string& term,
                                 uint32_t doc_id,
                                 const std::vector<uint32_t>& positions) {
    if (positions.empty())
        return;

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Add the positions to the buffer
    PositionEntry entry{doc_id, positions};
    position_buffer_[term].push_back(std::move(entry));

    // Update buffer size (approx)
    buffer_size_ += sizeof(uint32_t) + positions.size() * sizeof(uint32_t);

    // Flush if needed
    if (buffer_size_ >= MAX_BUFFER_SIZE) {
        flushBuffer(output_dir);
    }
}

void PositionIndex::addPositionsBatch(
    const std::string& output_dir,
    uint32_t doc_id,
    const std::vector<std::pair<std::string, std::vector<uint32_t>>>& term_positions) {

    if (term_positions.empty())
        return;

    // Single lock acquisition for all terms in the document
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t total_added_size = 0;
    for (const auto& [term, positions] : term_positions) {
        if (positions.empty())
            continue;

        // Create entry and directly move the positions vector
        PositionEntry entry{doc_id, positions};
        position_buffer_[term].push_back(std::move(entry));

        total_added_size += sizeof(uint32_t) + positions.size() * sizeof(uint32_t);
    }

    buffer_size_ += total_added_size;

    // Flush if needed
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

    // 2. Stopword filtering (should extend)
    static constexpr std::array stop_terms = {
        "the", "a", "an", "of", "in", "for", "on", "to", "with", "by", "at", "and", "or", "us", "have", "has"};
    if (std::find(stop_terms.begin(), stop_terms.end(), term) != stop_terms.end()) {
        return false;
    }

    // 3. freq-based filtering
    const bool is_common_term = freq > 3000;
    const bool is_doc_ubiquitous = (total_terms > 0) && (freq > total_terms / 8);
    if (is_common_term || is_doc_ubiquitous) {
        return false;
    }

    // 4. Minimum usefulness threshold
    return (freq >= 3);
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

        // Create buffer file
        std::string buffer_file = pos_dir + "/buffer_" + std::to_string(buffer_counter_++) + ".data";
        std::ofstream out(buffer_file, std::ios::binary);
        if (!out) {
            spdlog::error("Failed to create position buffer file: {}", buffer_file);
            return;
        }

        // Write number of terms
        uint32_t term_count = position_buffer_.size();
        out.write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));

        // Write each term's data
        for (const auto& [term, entries] : position_buffer_) {
            // Write term length and term
            uint32_t term_len = term.length();
            out.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
            out.write(term.data(), term_len);

            // Write doc count
            uint32_t doc_count = entries.size();
            out.write(reinterpret_cast<const char*>(&doc_count), sizeof(doc_count));

            // Write each doc's positions
            for (const auto& entry : entries) {
                // Write doc ID and position count
                out.write(reinterpret_cast<const char*>(&entry.doc_id), sizeof(entry.doc_id));
                uint32_t pos_count = entry.positions.size();
                out.write(reinterpret_cast<const char*>(&pos_count), sizeof(pos_count));

                // Write delta-encoded positions
                uint32_t prev_pos = 0;
                for (uint32_t pos : entry.positions) {
                    uint32_t delta = pos - prev_pos;
                    VByteCodec::encode(delta, out);
                    prev_pos = pos;
                }
            }
        }

        // Clear buffer
        position_buffer_.clear();
        buffer_size_ = 0;

    } catch (const std::exception& e) {
        spdlog::error("Error flushing position buffer: {}", e.what());
        // Reset buffer state to avoid infinite error loops
        position_buffer_.clear();
        buffer_size_ = 0;
    }
}

void PositionIndex::mergePositionBuffers(const std::string& output_dir) {
    std::string pos_dir = output_dir + "/positions";

    try {
        // Flush any remaining positions
        if (!position_buffer_.empty()) {
            flushBuffer(output_dir);
        }

        // Check if we have any buffer files
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

        // Structure to hold all term data - shared across threads
        std::unordered_map<std::string, std::vector<std::pair<uint32_t, std::vector<uint32_t>>>> term_data;
        std::mutex term_data_mutex;

        // Determine number of threads to use
        size_t num_threads = std::min(buffer_files.size(), static_cast<size_t>(std::thread::hardware_concurrency()));
        size_t files_per_thread = (buffer_files.size() + num_threads - 1) / num_threads;

        // Create threads to process buffer files in parallel
        std::vector<std::thread> threads;
        for (size_t t = 0; t < num_threads; t++) {
            size_t start_idx = t * files_per_thread;
            size_t end_idx = std::min(start_idx + files_per_thread, buffer_files.size());

            if (start_idx >= buffer_files.size())
                continue;

            threads.emplace_back([&, start_idx, end_idx]() {
                // Local map to collect data from this thread's files
                std::unordered_map<std::string, std::vector<std::pair<uint32_t, std::vector<uint32_t>>>>
                    local_term_data;

                // Process assigned files
                for (size_t i = start_idx; i < end_idx; i++) {
                    const auto& buffer_file = buffer_files[i];
                    std::ifstream in(buffer_file, std::ios::binary);
                    if (!in) {
                        spdlog::error("Failed to open buffer file: {}", buffer_file);
                        continue;
                    }

                    // Read term count
                    uint32_t term_count;
                    in.read(reinterpret_cast<char*>(&term_count), sizeof(term_count));

                    // Read each term
                    for (uint32_t i = 0; i < term_count && in.good(); i++) {
                        // Read term
                        uint32_t term_len;
                        in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
                        if (term_len > 1000) {  // Sanity check
                            spdlog::error("Invalid term length in buffer file: {}", term_len);
                            break;
                        }

                        std::string term(term_len, ' ');
                        in.read(&term[0], term_len);

                        // Read doc count
                        uint32_t doc_count;
                        in.read(reinterpret_cast<char*>(&doc_count), sizeof(doc_count));

                        // Read each doc's positions
                        for (uint32_t j = 0; j < doc_count && in.good(); j++) {
                            // Read doc ID and position count
                            uint32_t doc_id;
                            in.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
                            uint32_t pos_count;
                            in.read(reinterpret_cast<char*>(&pos_count), sizeof(pos_count));

                            if (pos_count > 100000) {  // Sanity check
                                spdlog::error("Invalid position count in buffer file: {}", pos_count);
                                break;
                            }

                            // Read positions
                            std::vector<uint32_t> positions;
                            positions.reserve(pos_count);
                            uint32_t prev_pos = 0;

                            for (uint32_t k = 0; k < pos_count && in.good(); k++) {
                                try {
                                    uint32_t delta = VByteCodec::decode(in);
                                    prev_pos += delta;
                                    positions.push_back(prev_pos);
                                } catch (const std::exception& e) {
                                    spdlog::error("Error decoding position: {}", e.what());
                                    break;
                                }
                            }

                            // Add to local term data
                            local_term_data[term].emplace_back(doc_id, std::move(positions));
                        }
                    }
                }

                // Merge local data into the shared term_data map
                {
                    std::lock_guard<std::mutex> lock(term_data_mutex);
                    for (auto& [term, doc_positions] : local_term_data) {
                        auto& target = term_data[term];
                        target.insert(target.end(),
                                      std::make_move_iterator(doc_positions.begin()),
                                      std::make_move_iterator(doc_positions.end()));
                    }
                }
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }

        // Create final position files
        std::string data_file = output_dir + "/positions.data";
        std::ofstream data_out(data_file, std::ios::binary);
        if (!data_out) {
            spdlog::error("Failed to create position data file: {}", data_file);
            return;
        }

        std::string posDict_file = output_dir + "/positions.dict";
        std::ofstream posDict_out(posDict_file, std::ios::binary);
        if (!posDict_out) {
            spdlog::error("Failed to create position dict file: {}", posDict_file);
            return;
        }

        // Reserve space for term count in dict
        uint32_t term_count = term_data.size();
        posDict_out.write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));

        // Process each term
        for (auto& [term, doc_positions] : term_data) {
            // Sort by doc ID
            std::sort(doc_positions.begin(), doc_positions.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });

            // Write term to dict
            uint32_t term_len = term.length();
            posDict_out.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
            posDict_out.write(term.data(), term_len);

            // Record position in data file
            PositionMetadata metadata;
            metadata.data_offset = static_cast<uint64_t>(data_out.tellp());
            metadata.doc_count = doc_positions.size();
            metadata.total_positions = 0;

            // Count total positions
            for (const auto& [doc_id, positions] : doc_positions) {
                metadata.total_positions += positions.size();
            }

            // Write metadata to dict
            posDict_out.write(reinterpret_cast<const char*>(&metadata.data_offset), sizeof(metadata.data_offset));
            posDict_out.write(reinterpret_cast<const char*>(&metadata.doc_count), sizeof(metadata.doc_count));
            posDict_out.write(reinterpret_cast<const char*>(&metadata.total_positions),
                              sizeof(metadata.total_positions));

            // Write positions to data file
            for (const auto& [doc_id, positions] : doc_positions) {
                // Write doc ID and position count
                data_out.write(reinterpret_cast<const char*>(&doc_id), sizeof(doc_id));
                uint32_t pos_count = positions.size();
                data_out.write(reinterpret_cast<const char*>(&pos_count), sizeof(pos_count));

                // Write delta-encoded positions
                uint32_t prev_pos = 0;
                std::vector<uint32_t> deltas;
                deltas.reserve(positions.size());
                for (uint32_t pos : positions) {
                    deltas.push_back(pos - prev_pos);
                    prev_pos = pos;
                }
                VByteCodec::encodeBatch(deltas, data_out);
            }
        }

        // Delete buffer files
        for (const auto& buffer_file : buffer_files) {
            fs::remove(buffer_file);
        }

        // Delete buffer directory
        fs::remove_all(pos_dir);

        spdlog::info("Position index created with {} terms", term_count);

    } catch (const std::exception& e) {
        spdlog::error("Error merging position buffers: {}", e.what());
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
        std::vector<uint32_t> positions = getPositions(term, doc_id);
        return !positions.empty();
    } catch (const std::exception& e) {
        spdlog::error("Error checking positions: {}", e.what());
        return false;
    }
}

std::vector<uint32_t> PositionIndex::getPositions(const std::string& term, uint32_t doc_id) const {
    // Check if term exists in dict
    auto it = posDict_.find(term);
    if (it == posDict_.end()) {
        return {};
    }

    try {
        // Get position metadata
        const PositionMetadata& metadata = it->second;

        // Seek to term's data
        data_file_.seekg(metadata.data_offset);
        if (!data_file_.good()) {
            spdlog::error("Failed to seek in position data file");
            return {};
        }

        // For each doc
        for (uint32_t i = 0; i < metadata.doc_count && data_file_.good(); i++) {
            // Read doc ID and position count
            uint32_t curr_doc_id;
            data_file_.read(reinterpret_cast<char*>(&curr_doc_id), sizeof(curr_doc_id));
            uint32_t pos_count;
            data_file_.read(reinterpret_cast<char*>(&pos_count), sizeof(pos_count));

            // Check if this is the doc we're looking for
            if (curr_doc_id == doc_id) {
                // Read positions
                std::vector<uint32_t> positions;
                positions.reserve(pos_count);

                // Read all deltas first (thats how we stored them)
                std::vector<uint32_t> deltas(pos_count);
                for (uint32_t j = 0; j < pos_count && data_file_.good(); j++) {
                    deltas[j] = VByteCodec::decode(data_file_);
                }

                // Then reconstruct positions
                uint32_t prev_pos = 0;
                for (uint32_t delta : deltas) {
                    prev_pos += delta;
                    positions.push_back(prev_pos);
                }

                return positions;
            }

            // Skip positions for this doc
            for (uint32_t j = 0; j < pos_count && data_file_.good(); j++) {
                VByteCodec::decode(data_file_);
            }
        }

        return {};
    } catch (const std::exception& e) {
        spdlog::error("Error getting positions: {}", e.what());
        return {};
    }
}

bool PositionIndex::checkPhrase(const std::string& term1,
                                const std::string& term2,
                                uint32_t doc_id,
                                int distance) const {
    try {
        // Get positions for both terms
        std::vector<uint32_t> positions1 = getPositions(term1, doc_id);
        std::vector<uint32_t> positions2 = getPositions(term2, doc_id);

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

}  // namespace mithril
