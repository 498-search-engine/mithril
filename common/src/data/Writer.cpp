#include "data/Writer.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <utility>
#include "core/pair.h"
#include <vector>

namespace mithril::data {

FileWriter::FileWriter(const char* filename) : file_(fopen(filename, "wb")), owned_(true) {
    if (file_ == nullptr) {
        throw std::runtime_error("Failed to open file: " + std::string{filename});
    }
}

FileWriter::FileWriter(FILE* f, bool takeOwnership) : file_(f), owned_(takeOwnership) {
    if (!file_) {
        throw std::runtime_error("Invalid file handle");
    }
}

FileWriter::~FileWriter() {
    if (owned_) {
        Close();
    }
}

FileWriter::FileWriter(FileWriter&& other) noexcept : file_(other.file_), owned_(other.owned_) {
    other.file_ = nullptr;
    other.owned_ = false;
}

FileWriter& FileWriter::operator=(FileWriter&& other) noexcept {
    if (this != &other) {
        if (owned_ && file_) {
            fclose(file_);
        }
        file_ = other.file_;
        owned_ = other.owned_;
        other.file_ = nullptr;
        other.owned_ = false;
    }
    return *this;
}

void FileWriter::Write(const void* data, size_t size) {
    if (size == 0) {
        return;
    }
    assert(file_ != nullptr);
    if (fwrite(data, 1, size, file_) != size) {
        auto err = ferror(file_);
        if (err) {
            throw std::runtime_error(std::string{"Failed to write to file: "} + std::strerror(errno));
        } else {
            throw std::runtime_error("Failed to write to file: unexpected write size");
        }
    }
}

void FileWriter::Write(char byte) {
    assert(file_ != nullptr);
    int res = fputc(byte, file_);
    if (res == EOF) {
        throw std::runtime_error(std::string{"Failed to write to file: "} + std::strerror(errno));
    }
}

long FileWriter::Ftell() const {
    assert(file_ != nullptr);
    auto pos = ftell(file_);
    if (pos < 0) {
        throw std::runtime_error(std::string{"Failed to get file position: "} + std::strerror(errno));
    } else {
        return pos;
    }
}

void FileWriter::Fseek(long pos, int origin) {
    assert(file_ != nullptr);
    if (fseek(file_, pos, origin) != 0) {
        throw std::runtime_error(std::string{"Failed to seek file: "} + std::strerror(errno));
    }
}

void FileWriter::Flush() {
    assert(file_ != nullptr);
    if (fflush(file_) != 0) {
        throw std::runtime_error("Failed to flush file");
    }
}

void FileWriter::Close() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
}

void FileWriter::DontNeed() {
#if defined(__linux__) || defined(__unix__)
    auto fd = fileno(file_);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
#endif
}

void BufferWriter::Write(const void* data, size_t size) {
    if (size == 0) {
        return;
    }
    const auto* charPtr = static_cast<const char*>(data);
    buffer_.insert(buffer_.end(), charPtr, charPtr + size);
}

std::vector<char> BufferWriter::Release() {
    std::vector<char> temp;
    std::swap(buffer_, temp);
    return temp;
}

}  // namespace mithril::data
