#ifndef POSITION_INDEX_H
#define POSITION_INDEX_H

#include "Utils.h"

#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mithril {

struct PositionMetadata {
    uint64_t data_offset;      // in positions.data file
    uint32_t doc_count;        // num of doc with this term
    uint32_t total_positions;  // across all docs
};

struct PositionEntry {
    uint32_t doc_id;
    std::vector<uint32_t> positions;
};

class PositionIndex {
public:
    // Constructor for querying
    PositionIndex(const std::string& index_dir);
    ~PositionIndex();

    // Query methods
    bool hasPositions(const std::string& term, uint32_t doc_id) const;
    std::vector<uint32_t> getPositions(const std::string& term, uint32_t doc_id) const;
    bool checkPhrase(const std::string& term1, const std::string& term2, uint32_t doc_id, int distance = 1) const;

    // Indexing methods
    static void addPositions(const std::string& output_dir,
                             const std::string& term,
                             uint32_t doc_id,
                             const std::vector<uint32_t>& positions);
    static void addPositionsBatch(const std::string& output_dir,
                                  uint32_t doc_id,
                                  const std::vector<std::pair<std::string, std::vector<uint32_t>>>& term_positions);

    static void finalizeIndex(const std::string& output_dir);
    static bool shouldStorePositions(const std::string& term, uint32_t freq, size_t total_terms);

private:
    // For querying
    std::string index_dir_;
    std::unordered_map<std::string, PositionMetadata> posDict_;
    mutable std::ifstream data_file_;
    bool loadPosDict();

    // Thread-safe position buffer
    static std::mutex buffer_mutex_;
    static std::unordered_map<std::string, std::vector<PositionEntry>> position_buffer_;
    static size_t buffer_size_;
    static int buffer_counter_;
    static constexpr size_t MAX_BUFFER_SIZE = 512 * 1024 * 1024;

    static void flushBuffer(const std::string& output_dir);
    static void mergePositionBuffers(const std::string& output_dir);
};

}  // namespace mithril

#endif  // POSITION_INDEX_H
