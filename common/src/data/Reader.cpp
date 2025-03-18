#include "data/Reader.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

namespace mithril::data {

FileReader::FileReader(const char* filename) : file_(fopen(filename, "rb")), owned_(true) {
    if (file_ == nullptr) {
        throw std::runtime_error("Failed to open file: " + std::string{filename});
    }
}

FileReader::FileReader(FILE* f, bool takeOwnership) : file_(f), owned_(takeOwnership) {}


FileReader::~FileReader() {
    if (owned_) {
        Close();
    }
}

FileReader::FileReader(FileReader&& other) noexcept : file_(other.file_), owned_(other.owned_) {
    other.file_ = nullptr;
    other.owned_ = false;
}

FileReader& FileReader::operator=(FileReader&& other) noexcept {
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

bool FileReader::Read(void* data, size_t size) {
    return fread(data, 1, size, file_) == size;
}

size_t FileReader::Remaining() {
    long current = ftell(file_);
    fseek(file_, 0, SEEK_END);
    long end = ftell(file_);
    fseek(file_, current, SEEK_SET);
    return end - current;
}

void FileReader::Close() {
    if (file_ != nullptr) {
        fclose(file_);
        file_ = nullptr;
    }
}

BufferReader::BufferReader(std::span<const char> d) : data_(d) {}

bool BufferReader::Read(void* out, size_t size) {
    if (position_ + size > data_.size()) {
        return false;
    }
    memcpy(out, Data(), size);
    SeekForward(size);
    return true;
}

size_t BufferReader::Remaining() {
    return data_.size() - position_;
}

const char* BufferReader::Data() const {
    return data_.data() + position_;
}

void BufferReader::SeekForward(size_t amount) {
    position_ += amount;
}


}  // namespace mithril::data
