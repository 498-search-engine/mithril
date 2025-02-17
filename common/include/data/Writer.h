#ifndef COMMON_DATA_WRITER_H
#define COMMON_DATA_WRITER_H

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace mithril::data {

template<typename T>
concept Writer = requires(T writer, const void* data, size_t size) {
    { writer.Write(data, size) } -> std::same_as<void>;
};

struct NopWriter {
    void Write(const void* data, size_t size) {}
};

class FileWriter {
public:
    FileWriter(const char* filename);
    FileWriter(FILE* f, bool takeOwnership = false);
    ~FileWriter();

    // Disable copying
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    // Allow moving
    FileWriter(FileWriter&& other) noexcept;
    FileWriter& operator=(FileWriter&& other) noexcept;

    void Write(const void* data, size_t size);
    void Flush();
    void Close();

private:
    FILE* file_;
    bool owned_;
};

class BufferWriter {
public:
    void Write(const void* data, size_t size);
    std::vector<char> Release();

private:
    std::vector<char> buffer_;
};


}  // namespace mithril::data

#endif
