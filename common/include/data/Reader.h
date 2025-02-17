#ifndef COMMON_DATA_READER_H
#define COMMON_DATA_READER_H

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace mithril::data {

template<typename T>
concept Reader = requires(T reader, void* data, size_t size) {
    // Read exactly 'size' bytes into data, returns false if couldn't read enough
    { reader.Read(data, size) } -> std::same_as<bool>;
    // How many bytes remain
    { reader.Remaining() } -> std::same_as<size_t>;
};

struct NopReader {
    bool Read(const void* /*data*/, size_t /*size*/) { return false; }
    size_t Remaining() { return 0; }
};

class FileReader {
public:
    FileReader(FILE* f);

    bool Read(void* data, size_t size);
    size_t Remaining();

private:
    FILE* file_;
};

class BufferReader {
public:
    BufferReader(std::span<const char> d);

    bool Read(void* out, size_t size);
    size_t Remaining();

private:
    std::span<const char> data_;
    size_t position_ = 0;
};

}  // namespace mithril::data

#endif
