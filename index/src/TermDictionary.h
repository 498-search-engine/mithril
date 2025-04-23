#ifndef INDEX_TERMDICTIONARY_H
#define INDEX_TERMDICTIONARY_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mithril {

class TermDictionary {
public:
    struct TermEntry {
        std::string term;
        uint64_t index_offset;
        uint32_t postings_count;
    };

    explicit TermDictionary(const std::string& index_dir);
    ~TermDictionary();

    std::optional<TermEntry> lookup(const std::string& term) const;
    size_t size() const { return term_count_; }
    bool is_loaded() const { return loaded_; }

private:
    int dict_fd_ = -1;
    const char* dict_data_ = nullptr;
    size_t dict_size_ = 0;

    uint32_t term_count_ = 0;
    uint32_t version_ = 0;

    // simple first-letter index to speed up lookups
    std::array<uint32_t, 256> first_char_index_{};
    std::vector<uint32_t> entry_offsets_;
    bool loaded_ = false;

    std::optional<TermEntry> search(const std::string& term) const;

    TermDictionary(const TermDictionary&) = delete;
    TermDictionary& operator=(const TermDictionary&) = delete;
};

}  // namespace mithril

#endif  // INDEX_TERMDICTIONARY_H
