#ifndef INDEX_TERMDICTIONARY_H
#define INDEX_TERMDICTIONARY_H

#include <cstdint>
#include <fstream>
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

    std::optional<TermEntry> lookup(const std::string& term) const;
    size_t size() const { return entries_.size(); }
    bool is_loaded() const { return loaded_; }

private:
    std::vector<TermEntry> entries_;
    bool loaded_{false};

    std::optional<TermEntry> binary_search(const std::string& term) const;
};

}  // namespace mithril

#endif  // INDEX_TERMDICTIONARY_H
