#ifndef INDEX_POSTINGBLOCK_H
#define INDEX_POSTINGBLOCK_H

#include <string>
#include <vector>

namespace mithril {

struct Posting {
    uint32_t doc_id;
    uint32_t freq;
    uint32_t positions_offset;
};

struct SyncPoint {
    uint32_t doc_id;        // First document ID at this position
    uint32_t plist_offset;  // Offset from start of postings list
};

struct PositionSyncPoint {
    uint32_t pos_offset;   // Offset in the postns array
    uint32_t absolute_pos; // Reconstructed absolute position
};

struct PositionsStore {
    std::vector<uint32_t> all_positions;
    std::vector<PositionSyncPoint> sync_points;
};

class BlockReader {
public:
    std::string current_term;
    std::vector<Posting> current_postings;
    std::vector<SyncPoint> current_sync_points;
    bool has_next{true};

    explicit BlockReader(const std::string& path);
    ~BlockReader();
    void read_next();

    Posting* find_posting(uint32_t target_doc_id);
    PositionsStore current_positions;
    std::vector<uint32_t> get_positions(uint32_t doc_id);
    std::vector<uint32_t> get_positions_near(uint32_t doc_id, uint32_t target_position, uint32_t window_size);

    // Move support for priority queue
    BlockReader(BlockReader&& other) noexcept;
    BlockReader& operator=(BlockReader&& other) noexcept;

    BlockReader(const BlockReader&) = delete;
    BlockReader& operator=(const BlockReader&) = delete;

private:
    const char* data{nullptr};
    size_t size{0};
    int fd{-1};
    const char* current{nullptr};

    size_t find_nearest_position_sync_point(uint32_t target_position) const;
    bool validate_remaining(size_t needed) const { return (current + needed <= data + size); }
};
}  // namespace mithril


#endif  // INDEX_POSTINGBLOCK_H