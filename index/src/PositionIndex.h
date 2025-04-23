#ifndef POSITION_INDEX_H
#define POSITION_INDEX_H

#include "TextPreprocessor.h"
#include "Utils.h"
#include "core/mem_map_file.h"
#include "data/Writer.h"

#include <array>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mithril {

using TermPositions = std::vector<std::pair<uint32_t, std::pair<uint8_t, std::vector<uint16_t>>>>;

struct PositionMetadata {
    uint64_t data_offset;
    uint32_t doc_count;
    uint32_t total_positions;
};

struct PositionEntry {
    uint32_t doc_id;
    uint8_t field_flags;
    std::vector<uint16_t> positions;
};

struct PositionSyncPoint {
    uint32_t doc_id;
    uint64_t offset;
};

static constexpr size_t NUM_FIELDS = static_cast<size_t>(FieldType::DESC) + 1;
static_assert(NUM_FIELDS == 5, "Unexpected number of field types");

struct FieldPositions {
    std::array<std::vector<uint16_t>, NUM_FIELDS> positions;
    uint8_t field_flags{0};
};

class PositionIndex {
public:
    PositionIndex(const std::string& index_dir);
    ~PositionIndex();

    bool hasPositions(const std::string& term, uint32_t doc_id) const;
    std::vector<uint16_t> getPositions(const std::string& term, uint32_t doc_id) const;
    uint8_t getFieldFlags(const std::string& term, uint32_t doc_id) const;
    bool checkPhrase(const std::string& term1, const std::string& term2, uint32_t doc_id, int distance = 1) const;

    static void addPositions(const std::string& output_dir,
                             const std::string& term,
                             uint32_t doc_id,
                             uint8_t field_flags,
                             const std::vector<uint16_t>& positions);

    static void addPositionsBatch(const std::string& output_dir,
                                  uint32_t doc_id,
                                  const std::vector<std::pair<std::string, FieldPositions>>& term_positions);

    static void finalizeIndex(const std::string& output_dir);
    static bool shouldStorePositions(const std::string& term, uint32_t freq, size_t total_terms);

private:
    std::string index_dir_;
    std::unordered_map<std::string, PositionMetadata> posDict_;
    mutable core::MemMapFile data_file_;

    bool loadPosDict();
    uint32_t decodeVByte(const char*& ptr) const;

    // Map of term -> sync points
    std::unordered_map<std::string, std::vector<PositionSyncPoint>> sync_points_;
    bool loadSyncPoints();
    uint64_t findNearestSyncPoint(const std::string& term, uint32_t doc_id) const;

    // Helper struct for getPositions and getFieldFlags
    struct DocPositionData {
        uint8_t field_flags;
        std::vector<uint16_t> positions;
        bool found;
    };

    DocPositionData getDocPositionData(const std::string& term, uint32_t doc_id) const;

    static std::mutex buffer_mutex_;
    static std::unordered_map<std::string, std::vector<PositionEntry>> position_buffer_;
    static size_t buffer_size_;
    static int buffer_counter_;
    static constexpr size_t MAX_BUFFER_SIZE = 512 * 1024 * 1024;

    static void flushBuffer(const std::string& output_dir);
    static void mergePositionBuffers(const std::string& output_dir);
    static bool writeTerm(const std::string& term,
                          const TermPositions& docs_positions,
                          data::FileWriter& data_out,
                          data::FileWriter& posDict_out);
};

}  // namespace mithril
#endif  // POSITION_INDEX_H
