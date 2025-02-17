#include "data/Writer.h"

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
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
    assert(file_ != nullptr);
    if (fwrite(data, 1, size, file_) != size) {
        throw std::runtime_error("Failed to write to file");
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

void BufferWriter::Write(const void* data, size_t size) {
    const auto* charPtr = static_cast<const char*>(data);
    buffer_.insert(buffer_.end(), charPtr, charPtr + size);
}

std::vector<char> BufferWriter::Release() {
    std::vector<char> temp;
    std::swap(buffer_, temp);
    return temp;
}

}  // namespace mithril::data
