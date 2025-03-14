// TermDictionary.cpp
#include "TermDictionary.h"

#include <algorithm>
#include <iostream>

namespace mithril {

TermDictionary::TermDictionary(const std::string& index_dir) {
    std::string dict_path = index_dir + "/term_dictionary.bin";

    std::ifstream file(dict_path, std::ios::binary);
    if (!file) {
        std::cerr << "Dictionary file not found: " << dict_path << std::endl;
        return;
    }

    // Read header
    uint32_t magic, version, term_count;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&term_count), sizeof(term_count));

    if (magic != 0x4D495448) {
        std::cerr << "Invalid dictionary file format" << std::endl;
        return;
    }

    if (version != 1) {
        std::cerr << "Unsupported dictionary version: " << version << std::endl;
        return;
    }

    entries_.reserve(term_count);
    for (uint32_t i = 0; i < term_count; i++) {
        uint32_t term_len;
        file.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));

        std::string term(term_len, '\0');
        file.read(&term[0], term_len);

        uint64_t offset;
        file.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        uint32_t postings_count;
        file.read(reinterpret_cast<char*>(&postings_count), sizeof(postings_count));

        entries_.push_back({term, offset, postings_count});
    }

    std::cout << "Loaded term dictionary with " << entries_.size() << " terms" << std::endl;
    loaded_ = true;
}

std::optional<TermDictionary::TermEntry> TermDictionary::lookup(const std::string& term) const {
    return binary_search(term);
}

std::optional<TermDictionary::TermEntry> TermDictionary::binary_search(const std::string& term) const {
    if (entries_.empty()) {
        return std::nullopt;
    }

    auto it =
        std::lower_bound(entries_.begin(), entries_.end(), term, [](const TermEntry& entry, const std::string& target) {
            return entry.term < target;
        });

    if (it != entries_.end() && it->term == term) {
        return *it;
    }

    return std::nullopt;
}

}  // namespace mithril
