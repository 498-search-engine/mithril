#ifndef INDEX_POSTINGBLOCK_H
#define INDEX_POSTINGBLOCK_H

#include <string>
#include <vector>

namespace mithril {

struct Posting {
    uint32_t doc_id;
    uint32_t freq;
};

class BlockReader {
public:
    std::string current_term;
    std::vector<Posting> current_postings;
    bool has_next{true};

    explicit BlockReader(const std::string& path);
    ~BlockReader();
    void read_next();

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

    bool validate_remaining(size_t needed) const { return (current + needed <= data + size); }
};
}  // namespace mithril


#endif  // INDEX_POSTINGBLOCK_H