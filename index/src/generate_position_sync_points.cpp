#include "PositionIndex.h"
#include "TextPreprocessor.h"
#include "Utils.h"
#include "core/mem_map_file.h"
#include "data/Writer.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace mithril {

class SyncPointGenerator {
public:
    SyncPointGenerator(const std::string& index_dir, uint32_t base_interval = 128)
        : index_dir_(index_dir),
          base_interval_(base_interval),
          dict_file_(index_dir + "/positions.dict"),
          data_file_(index_dir + "/positions.data"),
          output_file_(index_dir + "/positions.sync") {}

    bool generate() {
        spdlog::info("Starting sync point generation with base interval {}", base_interval_);

        // Step 1: Load position dictionary
        auto dict = loadPositionDict();
        if (dict.empty()) {
            spdlog::error("Failed to load position dictionary");
            return false;
        }
        spdlog::info("Loaded dictionary with {} terms", dict.size());

        // Step 2: Memory map position data file
        core::MemMapFile data_file(data_file_);
        // Check if mapping was successful - data() will be nullptr if mapping failed
        if (data_file.data() == nullptr || data_file.size() == 0) {
            spdlog::error("Failed to memory map position data file");
            return false;
        }
        spdlog::info("Mapped position data file: {} bytes", data_file.size());

        // Step 3: Create temporary output file
        std::string temp_file = output_file_ + ".tmp";
        data::FileWriter out(temp_file.c_str());

        // Step 4: Write header (version + term count)
        try {
            const uint32_t version = 1;
            out.Write(reinterpret_cast<const char*>(&version), sizeof(version));
            const uint32_t term_count = dict.size();
            out.Write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));
        } catch (const std::exception& e) {
            spdlog::error("Failed to write header to output file: {}", e.what());
            return false;
        }

        // Step 5: Process each term
        size_t processed = 0;
        size_t total_sync_points = 0;
        const char* data_start = data_file.data();
        const char* data_end = data_file.data() + data_file.size();

        for (const auto& [term, metadata] : dict) {
            processed++;

            // Determine appropriate interval for this term
            uint32_t interval = calculateInterval(metadata.doc_count);
            if (interval == 0) {
                // Skip terms with too few documents
                continue;
            }

            // Generate sync points for this term
            std::vector<PositionSyncPoint> sync_points;
            sync_points.reserve(metadata.doc_count / interval + 1);

            // Position at term's data
            const char* ptr = data_start + metadata.data_offset;

            // Process all documents for this term
            for (uint32_t doc_idx = 0; doc_idx < metadata.doc_count && ptr < data_end; doc_idx++) {
                // Record sync point at intervals
                if (doc_idx % interval == 0) {
                    uint64_t current_offset = ptr - data_start;

                    // Get document ID
                    uint32_t doc_id;
                    std::memcpy(&doc_id, ptr, sizeof(doc_id));

                    // Add sync point
                    sync_points.push_back({doc_id, current_offset});
                }

                // Skip to next document
                // 1. Skip doc ID
                ptr += sizeof(uint32_t);

                // 2. Skip field flags
                ptr += sizeof(uint8_t);

                // 3. Read and skip position count
                uint32_t pos_count;
                std::memcpy(&pos_count, ptr, sizeof(pos_count));
                ptr += sizeof(pos_count);

                // 4. Skip all positions
                for (uint32_t i = 0; i < pos_count && ptr < data_end; i++) {
                    // Skip VByte encoded position
                    while (ptr < data_end && (*ptr & 0x80)) {
                        ptr++;
                    }
                    ptr++;  // Skip final byte
                }
            }

            try {
                // Write term to sync file
                uint32_t term_len = term.length();
                out.Write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
                out.Write(term.data(), term_len);

                // Write sync points
                uint32_t point_count = sync_points.size();
                out.Write(reinterpret_cast<const char*>(&point_count), sizeof(point_count));
                out.Write(reinterpret_cast<const char*>(sync_points.data()), point_count * sizeof(PositionSyncPoint));

                total_sync_points += point_count;
            } catch (const std::exception& e) {
                spdlog::error("Failed to write sync points for term '{}': {}", term, e.what());
                return false;
            }

            if (processed % 10000 == 0) {
                spdlog::info(
                    "Processed {}/{} terms ({:.1f}%)", processed, dict.size(), 100.0 * processed / dict.size());
            }
        }

        out.Close();

        // Step 6: Rename temporary file to final
        try {
            if (fs::exists(output_file_)) {
                fs::remove(output_file_);
            }
            fs::rename(temp_file, output_file_);
        } catch (const std::exception& e) {
            spdlog::error("Failed to rename temp file: {}", e.what());
            return false;
        }

        spdlog::info(
            "Successfully created sync points file with {} terms, {} total points", processed, total_sync_points);
        return true;
    }

private:
    // Calculate optimal interval based on document count
    uint32_t calculateInterval(uint32_t doc_count) const {
        // Skip very small lists - no optimization needed
        if (doc_count < 16)
            return 0;

        // Adaptive intervals based on list size
        if (doc_count < 100)
            return 16;
        if (doc_count < 1000)
            return 32;
        if (doc_count < 10000)
            return 64;
        return base_interval_;  // Default for very large lists
    }

    // Load the position dictionary
    std::unordered_map<std::string, PositionMetadata> loadPositionDict() const {
        std::unordered_map<std::string, PositionMetadata> dict;

        std::ifstream in(dict_file_, std::ios::binary);
        if (!in) {
            spdlog::error("Failed to open position dictionary: {}", dict_file_);
            return dict;
        }

        try {
            uint32_t term_count;
            in.read(reinterpret_cast<char*>(&term_count), sizeof(term_count));

            for (uint32_t i = 0; i < term_count && in.good(); i++) {
                uint32_t term_len;
                in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));

                std::string term(term_len, ' ');
                in.read(&term[0], term_len);

                PositionMetadata metadata;
                in.read(reinterpret_cast<char*>(&metadata.data_offset), sizeof(metadata.data_offset));
                in.read(reinterpret_cast<char*>(&metadata.doc_count), sizeof(metadata.doc_count));
                in.read(reinterpret_cast<char*>(&metadata.total_positions), sizeof(metadata.total_positions));

                dict[term] = metadata;
            }
        } catch (const std::exception& e) {
            spdlog::error("Error reading position dictionary: {}", e.what());
            dict.clear();
        }

        return dict;
    }

    std::string index_dir_;
    uint32_t base_interval_;
    std::string dict_file_;
    std::string data_file_;
    std::string output_file_;
};

}  // namespace mithril

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <index_dir> [sync_interval=128]\n";
        return 1;
    }

    std::string index_dir = argv[1];
    uint32_t sync_interval = (argc > 2) ? std::stoi(argv[2]) : 128;

    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);

    spdlog::info("Building position sync index for {} with interval {}", index_dir, sync_interval);

    mithril::SyncPointGenerator generator(index_dir, sync_interval);
    if (!generator.generate()) {
        spdlog::error("Failed to generate position sync index");
        return 1;
    }

    return 0;
}