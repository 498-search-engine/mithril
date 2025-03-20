#ifndef INDEX_POSTINGBLOCK_H
#define INDEX_POSTINGBLOCK_H

#include <cstdint>
#include <string>
#include <vector>

namespace mithril {

struct Posting {
    uint32_t doc_id;
    uint32_t freq;
};

struct SyncPoint {
    uint32_t doc_id;        // First document ID at this position
    uint32_t plist_offset;  // Offset from start of postings list
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

    Posting* find_posting(uint32_t target_doc_id) const;

    // Move support for priority queue
    BlockReader(BlockReader&& other) noexcept;
    BlockReader& operator=(BlockReader&& other) noexcept;

    BlockReader(const BlockReader&) = delete;
    BlockReader& operator=(const BlockReader&) = delete;

    std::string getFilePath() const { return file_path_; }

private:
    const char* data{nullptr};
    size_t size{0};
    int fd{-1};
    const char* current{nullptr};
    std::string file_path_;
    // std::vector<char> file_buffer_;

    bool validate_remaining(size_t needed) const { return (current + needed <= data + size); }
};
}  // namespace mithril


#endif  // INDEX_POSTINGBLOCK_H
