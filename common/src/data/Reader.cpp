#include "data/Reader.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace mithril::data {

FileReader::FileReader(FILE* f) : file_(f) {}

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

BufferReader::BufferReader(std::span<const char> d) : data_(d) {}

bool BufferReader::Read(void* out, size_t size) {
    if (position_ + size > data_.size()) {
        return false;
    }
    memcpy(out, data_.data() + position_, size);
    position_ += size;
    return true;
}

size_t BufferReader::Remaining() {
    return data_.size() - position_;
}

}  // namespace mithril::data
