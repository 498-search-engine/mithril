#include "PostingBlock.h"

#include "InvertedIndex.h"
#include "Utils.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril {

BlockReader::BlockReader(const std::string& path) : file_path_(path) {
    fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "ERROR: Open failed: " << strerror(errno) << std::endl;
        throw std::runtime_error("Open failed: " + std::string(strerror(errno)));
    }

    // Get file size using stat instead of lseek
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        std::cerr << "ERROR: Failed to get file size: " << strerror(errno) << std::endl;
        throw std::runtime_error("Failed to get file size: " + std::string(strerror(errno)));
    }
    size = sb.st_size;

    // Map file into memory (more efficient than reading)
    data = static_cast<const char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        close(fd);
        data = nullptr;  // Ensure data is null if mapping failed
        throw std::runtime_error("Memory mapping failed: " + std::string(strerror(errno)));
    }

    current = data;

    // Skip initial term count
    current += sizeof(uint32_t);

    read_next();
}

BlockReader::~BlockReader() {
    if (data != MAP_FAILED && data != nullptr) {
        munmap(const_cast<char*>(data), size);
    }
    if (fd != -1) {
        close(fd);
    }
}

BlockReader::BlockReader(BlockReader&& other) noexcept
    : current_term(std::move(other.current_term)),
      current_postings(std::move(other.current_postings)),
      current_sync_points(std::move(other.current_sync_points)),
      data(other.data),
      size(other.size),
      fd(other.fd),
      current(other.current),
      has_next(other.has_next),
      file_path_(std::move(other.file_path_)) {
    other.data = nullptr;
    other.size = 0;
    other.fd = -1;
    other.current = nullptr;
    other.has_next = false;
}

BlockReader& BlockReader::operator=(BlockReader&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        if (data != MAP_FAILED && data != nullptr) {
            munmap(const_cast<char*>(data), size);
        }
        if (fd != -1) {
            close(fd);
        }

        // Move from other
        current_term = std::move(other.current_term);
        current_postings = std::move(other.current_postings);
        current_sync_points = std::move(other.current_sync_points);
        data = other.data;
        size = other.size;
        fd = other.fd;
        current = other.current;
        has_next = other.has_next;
        file_path_ = std::move(other.file_path_);

        // Reset other
        other.data = nullptr;
        other.size = 0;
        other.fd = -1;
        other.current = nullptr;
        other.has_next = false;
    }
    return *this;
}

void BlockReader::read_next() {
    if (!validate_remaining(sizeof(uint32_t))) {
        has_next = false;
        return;
    }

    // Read term length and validate
    uint32_t term_len;
    std::memcpy(&term_len, current, sizeof(term_len));
    current += sizeof(term_len);
    if (!validate_remaining(term_len + sizeof(uint32_t))) {
        has_next = false;
        return;
    }

    // Read term
    current_term.assign(current, term_len);
    current += term_len;

    // Read postings size and validate
    uint32_t postings_size;
    std::memcpy(&postings_size, current, sizeof(postings_size));
    current += sizeof(postings_size);

    // Read sync points size
    uint32_t sync_points_size;
    std::memcpy(&sync_points_size, current, sizeof(sync_points_size));
    current += sizeof(sync_points_size);

    // Read sync points if any
    current_sync_points.resize(sync_points_size);
    if (sync_points_size > 0) {
        if (!validate_remaining(sync_points_size * sizeof(SyncPoint))) {
            has_next = false;
            return;
        }
        std::memcpy(current_sync_points.data(), current, sync_points_size * sizeof(SyncPoint));
        current += sync_points_size * sizeof(SyncPoint);
    }

    if (!validate_remaining(postings_size * sizeof(Posting))) {
        has_next = false;
        return;
    }

    current_postings.resize(postings_size);
    std::memcpy(current_postings.data(), current, postings_size * sizeof(Posting));
    current += postings_size * sizeof(Posting);
}

Posting* BlockReader::find_posting(uint32_t target_doc_id) const {
    if (current_postings.empty() || target_doc_id < current_postings.front().doc_id ||
        target_doc_id > current_postings.back().doc_id) {
        return nullptr;
    }

    // Use sync points to find starting position for search
    size_t start_pos = 0;
    if (!current_sync_points.empty()) {
        int left = 0;
        int right = current_sync_points.size() - 1;

        while (left <= right) {
            // binary search for sync point
            int mid = left + (right - left) / 2;
            if (current_sync_points[mid].doc_id <= target_doc_id &&
                (mid == right || current_sync_points[mid + 1].doc_id > target_doc_id)) {
                start_pos = current_sync_points[mid].plist_offset;
                break;
            } else if (current_sync_points[mid].doc_id > target_doc_id) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        }
    }

    // Linear search from selected sync point
    for (size_t i = start_pos; i < current_postings.size(); i++) {
        if (current_postings[i].doc_id == target_doc_id) {
            return const_cast<Posting*>(&current_postings[i]);
        } else if (current_postings[i].doc_id > target_doc_id) {
            break;
        }
    }

    return nullptr;
}

}  // namespace mithril
