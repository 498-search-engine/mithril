#include "PostingBlock.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril {

BlockReader::BlockReader(const std::string& path) {
    fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open block file: " + std::string(strerror(errno)));
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        throw std::runtime_error("Failed to stat block file: " + std::string(strerror(errno)));
    }

    size = sb.st_size;
    if (size < sizeof(uint32_t)) {
        close(fd);
        throw std::runtime_error("Block file too small");
    }

    data = static_cast<const char*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    if (data == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("Failed to mmap block file: " + std::string(strerror(errno)));
    }

    current = data;

    uint32_t num_terms;
    std::memcpy(&num_terms, current, sizeof(num_terms));
    current += sizeof(num_terms);

    madvise(const_cast<char*>(data), size, MADV_SEQUENTIAL);

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
      has_next(other.has_next),
      data(other.data),
      size(other.size),
      fd(other.fd),
      current(other.current) {
    other.data = nullptr;
    other.fd = -1;
}

BlockReader& BlockReader::operator=(BlockReader&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        if (data != MAP_FAILED && data != nullptr) {
            munmap(const_cast<char*>(data), size);
        }
        if (fd != -1) {
            close(fd);
        }

        // Move data
        current_term = std::move(other.current_term);
        current_postings = std::move(other.current_postings);
        has_next = other.has_next;
        data = other.data;
        size = other.size;
        fd = other.fd;
        current = other.current;

        other.data = nullptr;
        other.fd = -1;
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

    if (!validate_remaining(postings_size * sizeof(Posting))) {
        has_next = false;
        return;
    }

    // Read postings
    current_postings.resize(postings_size);
    std::memcpy(current_postings.data(), current, postings_size * sizeof(Posting));
    current += postings_size * sizeof(Posting);
}

}  // namespace mithril
