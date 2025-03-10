#include "PostingBlock.h"

#include "InvertedIndex.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril {

BlockReader::BlockReader(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Open failed: " + std::string(strerror(errno)));
    }

    // Get file size using lseek
    const off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    // Bulk read entire file into buffer
    file_buffer_.resize(file_size);
    const ssize_t bytes_read = read(fd, file_buffer_.data(), file_size);
    close(fd);
    
    if (bytes_read != file_size) {
        throw std::runtime_error("Read failed: " + std::string(strerror(errno)));
    }

    // Use buffer directly
    data = file_buffer_.data();
    size = file_buffer_.size();
    current = data;

    // Skip initial term count (not needed for sequential read)
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
    : file_buffer_(std::move(other.file_buffer_)),
      current_term(std::move(other.current_term)),
      current_postings(std::move(other.current_postings)),
      current_positions(std::move(other.current_positions)),
      data(file_buffer_.data()),
      size(file_buffer_.size()),
      current(other.current),
      has_next(other.has_next) {
    other.data = nullptr;
    other.size = 0;
    other.current = nullptr;
}

BlockReader& BlockReader::operator=(BlockReader&& other) noexcept {
    if (this != &other) {
        file_buffer_ = std::move(other.file_buffer_);  // Move buffer first
        data = file_buffer_.data();  // Update pointers after buffer is moved
        size = file_buffer_.size();
        
        current_term = std::move(other.current_term);
        current_postings = std::move(other.current_postings);
        current_positions = std::move(other.current_positions);
        current = other.current;
        has_next = other.has_next;

        other.data = nullptr;
        other.size = 0;
        other.current = nullptr;
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

    // Read positions
    uint32_t positions_size;
    std::memcpy(&positions_size, current, sizeof(positions_size));
    current += sizeof(positions_size);

    // Read position sync points
    uint32_t position_sync_points_size;
    std::memcpy(&position_sync_points_size, current, sizeof(position_sync_points_size));
    current += sizeof(position_sync_points_size);

    current_positions.sync_points.resize(position_sync_points_size);
    if (position_sync_points_size > 0) {
        std::memcpy(
            current_positions.sync_points.data(), current, position_sync_points_size * sizeof(PositionSyncPoint));
        current += position_sync_points_size * sizeof(PositionSyncPoint);
    }

    // (opt?) Read VByte encoded positions
    current_positions.all_positions.resize(positions_size);
    const char* pos_ptr = current;
    for (size_t i = 0; i < positions_size; ++i) {
        current_positions.all_positions[i] = VByteCodec::decode_from_memory(pos_ptr);
    }
    current = pos_ptr;  // Bulk advance pointer after all decodes
}

Posting* BlockReader::find_posting(uint32_t target_doc_id) {
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
            return &current_postings[i];
        } else if (current_postings[i].doc_id > target_doc_id) {
            break;
        }
    }

    return nullptr;
}

std::vector<uint32_t> BlockReader::get_positions(uint32_t doc_id) {
    Posting* posting = find_posting(doc_id);
    if (!posting || posting->positions_offset == UINT32_MAX)
        return {};

    // Calculate positions range
    size_t index = posting - current_postings.data();
    size_t start = posting->positions_offset;
    size_t end = (index == current_postings.size() - 1) ? current_positions.all_positions.size()
                                                        : current_postings[index + 1].positions_offset;

    // Reconstruct absolute positions from deltas
    std::vector<uint32_t> absolute_positions;
    absolute_positions.reserve(end - start);
    uint32_t current_pos = 0;
    for (size_t i = start; i < end; i++) {
        current_pos += current_positions.all_positions[i];  // Add delta
        absolute_positions.push_back(current_pos);
    }

    return absolute_positions;
}

std::vector<uint32_t> BlockReader::get_positions_near(uint32_t doc_id, uint32_t target_position, uint32_t window_size) {
    Posting* posting = find_posting(doc_id);
    if (!posting || posting->positions_offset == UINT32_MAX)
        return {};

    // Calculate positions range
    size_t index = posting - current_postings.data();
    size_t start = posting->positions_offset;
    size_t end = (index == current_postings.size() - 1) ? current_positions.all_positions.size()
                                                        : current_postings[index + 1].positions_offset;

    // Find closest sync point to target position
    size_t sync_start = start;
    uint32_t current_pos = 0;
    bool found_sync_point = false;

    // Find the latest sync point that's before or at our target position
    for (const auto& sync_point : current_positions.sync_points) {
        if (sync_point.pos_offset >= start && sync_point.pos_offset < end &&
            sync_point.absolute_pos <= target_position) {
            sync_start = sync_point.pos_offset;
            current_pos = sync_point.absolute_pos;
            found_sync_point = true;
        } else if (sync_point.pos_offset >= start && sync_point.pos_offset < end &&
                   sync_point.absolute_pos > target_position) {
            // We've gone past the target, stop looking
            break;
        }
    }

    // If no suitable sync point was found but we're not starting from the beginning,
    // we need to calculate positions from the start
    if (!found_sync_point && sync_start > start) {
        for (size_t i = start; i < sync_start; i++) {
            current_pos += current_positions.all_positions[i];
        }
    }

    // Scan forward to find positions within window
    std::vector<uint32_t> positions_near;
    for (size_t i = sync_start; i < end; i++) {
        current_pos += current_positions.all_positions[i];

        // Check if position is within window
        if (std::abs(static_cast<int64_t>(current_pos) - static_cast<int64_t>(target_position)) <= window_size) {
            positions_near.push_back(current_pos);
        }

        // Stop scanning if we're past the window
        if (current_pos > target_position + window_size) {
            break;
        }
    }

    return positions_near;
}

}  // namespace mithril
