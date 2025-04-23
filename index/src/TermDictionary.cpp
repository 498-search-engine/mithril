#include "TermDictionary.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril {

TermDictionary::TermDictionary(const std::string& index_dir) {
    std::string dict_path = index_dir + "/term_dictionary.data";
    spdlog::info("constructing term dictionary for {}", index_dir);

    dict_fd_ = open(dict_path.c_str(), O_RDONLY);
    if (dict_fd_ == -1) {
        std::cerr << "Dictionary file not found: " << dict_path << std::endl;
        return;
    }

    struct stat sb;
    if (fstat(dict_fd_, &sb) == -1) {
        std::cerr << "Failed to get dictionary file size" << std::endl;
        close(dict_fd_);
        dict_fd_ = -1;
        return;
    }

    dict_size_ = sb.st_size;

    dict_data_ = static_cast<const char*>(mmap(nullptr, dict_size_, PROT_READ, MAP_PRIVATE, dict_fd_, 0));
    if (dict_data_ == MAP_FAILED) {
        std::cerr << "Failed to memory map dictionary file" << std::endl;
        close(dict_fd_);
        dict_fd_ = -1;
        dict_data_ = nullptr;
        return;
    }

    // Force in memory
    if (mlock(dict_data_, dict_size_) != 0) {
        spdlog::warn("Failed to lock into memory dictionary file");
    }

    // Touch all pages to force in memory
    volatile char tmp;
    for (size_t i = 0; i < dict_size_; i += 4096) {
        tmp = *((char*)dict_data_ + i);
    }

    const char* ptr = dict_data_;
    uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);

    version_ = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);

    term_count_ = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);

    if (magic != 0x4D495448) {
        std::cerr << "Invalid dictionary file format" << std::endl;
        return;
    }

    if (version_ != 1) {
        std::cerr << "Unsupported dictionary version: " << version_ << std::endl;
        return;
    }

    // Build simple first-char index for faster lookups
    std::fill(first_char_index_.begin(), first_char_index_.end(), UINT32_MAX);

    const char* entry_ptr = ptr;
    for (uint32_t i = 0; i < term_count_; i++) {
        uint32_t term_len = *reinterpret_cast<const uint32_t*>(entry_ptr);
        entry_ptr += sizeof(uint32_t);

        // Record the first occurrence of each starting char
        if (term_len > 0) {
            unsigned char first_char = static_cast<unsigned char>(entry_ptr[0]);
            if (first_char_index_[first_char] == UINT32_MAX) {
                first_char_index_[first_char] = i;
            }
        }

        // Skip to next entry
        entry_ptr += term_len + sizeof(uint64_t) + sizeof(uint32_t);
    }

    entry_offsets_.reserve(term_count_);
    const auto* const base_data_ptr = dict_data_ + (3 * sizeof(uint32_t));  // Skip header
    uint32_t offset = 0;
    for (uint32_t i = 0; i < term_count_; i++) {
        entry_offsets_.push_back(offset);
        uint32_t term_len = *reinterpret_cast<const uint32_t*>(base_data_ptr + offset);
        offset += sizeof(uint32_t) + term_len + sizeof(uint64_t) + sizeof(uint32_t);
    }

    spdlog::info("Memory mapped term dictionary with {} terms", term_count_);
    loaded_ = true;
}

TermDictionary::~TermDictionary() {
    if (dict_data_ != nullptr && dict_data_ != MAP_FAILED) {
        munmap(const_cast<char*>(dict_data_), dict_size_);
    }

    if (dict_fd_ != -1) {
        close(dict_fd_);
    }
}

std::optional<TermDictionary::TermEntry> TermDictionary::lookup(const std::string& term) const {
    return search(term);
}

std::optional<TermDictionary::TermEntry> TermDictionary::search(const std::string& term) const {
    if (!loaded_ || term_count_ == 0 || term.empty()) {
        return std::nullopt;
    }

    // Use first char index to narrow search range
    unsigned char first_char = static_cast<unsigned char>(term[0]);
    uint32_t start_idx = first_char_index_[first_char];

    // If no terms start with this char, return null
    if (start_idx == UINT32_MAX) {
        return std::nullopt;
    }

    // Find the end of this char's range
    uint32_t end_idx = term_count_;
    for (size_t c = first_char + 1; c < 256; c++) {
        if (first_char_index_[c] != UINT32_MAX) {
            end_idx = first_char_index_[c];
            break;
        }
    }

    // ptr to the start of entries section
    const char* entries_start = dict_data_ + 3 * sizeof(uint32_t);  // Skip header
    // binary search within char range
    uint32_t left = start_idx;
    uint32_t right = end_idx - 1;

    while (left <= right) {
        uint32_t mid = left + (right - left) / 2;

        // Read term at mid
        const char* entry_ptr = entries_start + entry_offsets_[mid];
        uint32_t term_len = *reinterpret_cast<const uint32_t*>(entry_ptr);
        entry_ptr += sizeof(uint32_t);

        auto mid_term = std::string_view{entry_ptr, term_len};
        int comparison = term.compare(mid_term);

        if (comparison == 0) {
            // Found the term!
            entry_ptr += term_len;

            TermEntry entry;
            entry.term = mid_term;
            entry.index_offset = *reinterpret_cast<const uint64_t*>(entry_ptr);
            entry_ptr += sizeof(uint64_t);
            entry.postings_count = *reinterpret_cast<const uint32_t*>(entry_ptr);

            return entry;
        } else if (comparison < 0) {
            if (mid == 0)
                break;
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return std::nullopt;
}

}  // namespace mithril
