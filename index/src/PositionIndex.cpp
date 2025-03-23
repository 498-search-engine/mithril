#include "PositionIndex.h"

#include "TextPreprocessor.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
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

// Utility class to help with streaming merge
class BufferFileReader {
public:
    struct TermInfo {
        std::string term;
        uint32_t doc_count;
        std::streampos term_start_pos;

        bool operator<(const TermInfo& other) const { return term < other.term; }
    };

    BufferFileReader(const std::string& filename) {
        file_.open(filename, std::ios::binary);
        if (!file_) {
            throw std::runtime_error("Failed to open buffer file: " + filename);
        }

        // Read term count
        file_.read(reinterpret_cast<char*>(&term_count_), sizeof(term_count_));
        start_pos_ = file_.tellg();

        // Scan file for terms (but don't load positions)
        scanForTerms();
    }

    ~BufferFileReader() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool isOpen() const { return file_.is_open() && file_.good(); }

    const std::vector<TermInfo>& getTerms() const { return terms_; }

    // Read positions for a specific term
    std::vector<std::pair<uint32_t, std::vector<uint32_t>>> readTermPositions(const std::string& term) {
        std::vector<std::pair<uint32_t, std::vector<uint32_t>>> result;

        // Find term info
        auto it =
            std::find_if(terms_.begin(), terms_.end(), [&term](const TermInfo& info) { return info.term == term; });

        if (it == terms_.end()) {
            return result;  // Term not found
        }

        // Seek to term start position
        file_.seekg(it->term_start_pos);

        // Skip term length and term (already read during scan)
        uint32_t term_len;
        file_.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
        file_.seekg(term_len, std::ios::cur);

        // Read doc count
        uint32_t doc_count;
        file_.read(reinterpret_cast<char*>(&doc_count), sizeof(doc_count));

        // Read positions for each doc
        for (uint32_t i = 0; i < doc_count && file_.good(); i++) {
            // Read doc ID and position count
            uint32_t doc_id;
            file_.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));

            uint32_t pos_count;
            file_.read(reinterpret_cast<char*>(&pos_count), sizeof(pos_count));

            // Read positions
            std::vector<uint32_t> positions;
            positions.reserve(pos_count);
            uint32_t prev_pos = 0;

            for (uint32_t j = 0; j < pos_count && file_.good(); j++) {
                try {
                    uint32_t delta = VByteCodec::decode(file_);
                    prev_pos += delta;
                    positions.push_back(prev_pos);
                } catch (const std::exception& e) {
                    spdlog::error("Error decoding position: {}", e.what());
                    break;
                }
            }

            result.emplace_back(doc_id, std::move(positions));
        }

        return result;
    }

private:
    void scanForTerms() {
        std::streampos current_pos = start_pos_;
        file_.seekg(current_pos);

        for (uint32_t i = 0; i < term_count_ && file_.good(); i++) {
            // Save position for this term
            std::streampos term_start = file_.tellg();

            // Read term
            uint32_t term_len;
            file_.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
            if (term_len > 10000) {  // Sanity check
                spdlog::error("Invalid term length in buffer file: {}", term_len);
                break;
            }

            std::string term(term_len, ' ');
            file_.read(&term[0], term_len);

            // Read doc count
            uint32_t doc_count;
            file_.read(reinterpret_cast<char*>(&doc_count), sizeof(doc_count));

            // Store term info
            terms_.push_back({term, doc_count, term_start});

            // Skip position data for all docs for this term
            for (uint32_t j = 0; j < doc_count && file_.good(); j++) {
                // Skip doc ID
                file_.seekg(sizeof(uint32_t), std::ios::cur);

                // Read position count
                uint32_t pos_count;
                file_.read(reinterpret_cast<char*>(&pos_count), sizeof(pos_count));

                // Skip positions
                for (uint32_t k = 0; k < pos_count && file_.good(); k++) {
                    try {
                        VByteCodec::decode(file_);
                    } catch (const std::exception& e) {
                        spdlog::error("Error skipping position: {}", e.what());
                        break;
                    }
                }
            }
        }
    }

    std::ifstream file_;
    uint32_t term_count_;
    std::streampos start_pos_;
    std::vector<TermInfo> terms_;
};

bool PositionIndex::shouldStorePositions(const std::string& term, uint32_t freq, size_t total_terms) {
    // Always store positions for title, description, and proper nouns
    if (term.size() > 1 && (term[0] == '#' || term[0] == '%' || std::isupper(term[0]))) {
        return true;
    }

    // Never store positions for common stop terms
    static const std::unordered_set<std::string> stop_terms = {
        "the",   "a",      "an",   "of",    "in",    "for",   "on",    "to",   "with",  "by",   "at",  "is",   "are",
        "was",   "were",   "be",   "been",  "being", "have",  "has",   "had",  "do",    "does", "did", "will", "would",
        "shall", "should", "may",  "might", "must",  "can",   "could", "and",  "or",    "but",  "if",  "then", "else",
        "when",  "so",     "that", "this",  "these", "those", "them",  "they", "their", "there"};

    if (stop_terms.find(term) != stop_terms.end()) {
        return false;
    }

    // More aggressive filtering for common terms - changed from /5 to /10
    if (total_terms > 0 && freq > total_terms / 10) {
        return false;
    }

    return true;
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

        // Create output files
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

        // Open all buffer files and scan for terms
        std::vector<std::unique_ptr<BufferFileReader>> readers;
        std::set<std::string> all_terms;

        for (const auto& file : buffer_files) {
            try {
                auto reader = std::make_unique<BufferFileReader>(file);
                // Collect unique terms
                for (const auto& term_info : reader->getTerms()) {
                    all_terms.insert(term_info.term);
                }
                readers.push_back(std::move(reader));
            } catch (const std::exception& e) {
                spdlog::error("Error opening buffer file {}: {}", file, e.what());
            }
        }

        // Reserve space for term count in dict
        uint32_t term_count = all_terms.size();
        posDict_out.write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));

        // Process each term across all files - THE KEY STREAMING OPTIMIZATION
        int term_processed = 0;
        for (const auto& term : all_terms) {
            // Show progress every 10,000 terms
            if (++term_processed % 10000 == 0) {
                spdlog::info("Processed {}/{} terms", term_processed, term_count);
            }

            // Collect positions from all files for this term
            std::map<uint32_t, std::vector<uint32_t>> doc_positions;  // Map: doc_id -> positions

            for (auto& reader : readers) {
                if (!reader->isOpen())
                    continue;

                auto positions = reader->readTermPositions(term);
                for (auto& [doc_id, pos_vec] : positions) {
                    // Merge positions for the same document
                    auto& target = doc_positions[doc_id];
                    if (target.empty()) {
                        // First occurrence, just move
                        target = std::move(pos_vec);
                    } else {
                        // Merge and keep sorted
                        target.insert(target.end(), pos_vec.begin(), pos_vec.end());
                        std::sort(target.begin(), target.end());
                        // Remove duplicates if any
                        target.erase(std::unique(target.begin(), target.end()), target.end());
                    }
                }
            }

            // If no documents contain this term, skip it
            if (doc_positions.empty())
                continue;

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

                uint32_t prev_pos = 0;
                for (uint32_t j = 0; j < pos_count && data_file_.good(); j++) {
                    uint32_t delta = VByteCodec::decode(data_file_);
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
