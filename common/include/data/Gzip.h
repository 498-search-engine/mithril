#ifndef COMMON_DATA_GZIP_H
#define COMMON_DATA_GZIP_H

#include "data/Reader.h"
#include "data/Writer.h"

#include <algorithm>
#include <alloca.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <zconf.h>
#include <zlib.h>
#include <sys/types.h>

namespace mithril::data {

constexpr size_t GzipChunkSize = 16384;

template<Reader R>
class GzipReader {
public:
    explicit GzipReader(R& r) : underlying_(r), inBuffer_(GzipChunkSize), outBuffer_(GzipChunkSize) {
        strm_.zalloc = Z_NULL;
        strm_.zfree = Z_NULL;
        strm_.opaque = Z_NULL;
        strm_.avail_in = 0;
        strm_.next_in = Z_NULL;

        if (inflateInit2(&strm_, 31) != Z_OK) {  // 15 + 16 for gzip format
            throw std::runtime_error("Failed to initialize zlib");
        }
    }

    ~GzipReader() { inflateEnd(&strm_); }

    ssize_t ReadAmount(void* out, size_t size) {
        auto* outPtr = static_cast<uint8_t*>(out);
        size_t bytesRead = 0;

        while (bytesRead < size) {
            // If we have buffered output data, use it first
            if (outPos_ < outSize_) {
                size_t available = outSize_ - outPos_;
                size_t toCopy = std::min(available, size - bytesRead);
                memcpy(outPtr + bytesRead, outBuffer_.data() + outPos_, toCopy);
                outPos_ += toCopy;
                bytesRead += toCopy;
                continue;
            }

            if (eof_) {
                break;
            }

            // Read more input if needed
            if (strm_.avail_in == 0) {
                auto remaining = underlying_.Remaining();
                auto sizeToRead = std::min(remaining, inBuffer_.size());
                if (sizeToRead == 0) {
                    break;
                }

                if (!underlying_.Read(inBuffer_.data(), sizeToRead)) {
                    break;
                }

                strm_.avail_in = inBuffer_.size();
                strm_.next_in = reinterpret_cast<Bytef*>(inBuffer_.data());
            }

            // Decompress
            strm_.avail_out = outBuffer_.size();
            strm_.next_out = reinterpret_cast<Bytef*>(outBuffer_.data());

            int ret = inflate(&strm_, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) {
                eof_ = true;
            } else if (ret != Z_OK) {
                throw std::runtime_error("Zlib decompression error");
            }

            outSize_ = outBuffer_.size() - strm_.avail_out;
            outPos_ = 0;
        }

        return static_cast<int>(bytesRead);
    }

    bool Read(void* out, size_t size) { return ReadAmount(out, size) == size && size != 0; }

    size_t Remaining() { return 0; }

private:
    R& underlying_;

    z_stream strm_{};
    std::vector<char> inBuffer_;
    std::vector<char> outBuffer_;
    size_t outPos_ = 0;
    size_t outSize_ = 0;
    bool eof_ = false;
};


template<Writer W>
class GzipWriter {
public:
    explicit GzipWriter(W& w) : underlying_(w), buffer_(GzipChunkSize) {
        strm_.zalloc = Z_NULL;
        strm_.zfree = Z_NULL;
        strm_.opaque = Z_NULL;

        if (deflateInit2(&strm_,
                         Z_DEFAULT_COMPRESSION,
                         Z_DEFLATED,
                         31,  // 15 for max window size + 16 for gzip format
                         8,   // Default memory level
                         Z_DEFAULT_STRATEGY) != Z_OK) {
            throw std::runtime_error("Failed to initialize zlib");
        }
    }

    ~GzipWriter() {
        if (!finished_) {
            try {
                Finish();
            } catch (...) {  // NOLINT(bugprone-empty-catch)
                // Can't throw in destructor
            }
        }
        deflateEnd(&strm_);
    }

    void Write(const void* data, size_t size) {
        if (finished_) {
            throw std::runtime_error("Cannot write after finishing");
        }

        auto* x = alloca(size);
        std::memcpy(x, data, size);

        strm_.next_in = static_cast<Bytef*>(x);
        strm_.avail_in = size;

        do {
            strm_.next_out = reinterpret_cast<Bytef*>(buffer_.data());
            strm_.avail_out = buffer_.size();

            int ret = deflate(&strm_, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                throw std::runtime_error("Zlib compression error");
            }

            size_t have = buffer_.size() - strm_.avail_out;
            if (have > 0) {
                underlying_.Write(buffer_.data(), have);
            }
        } while (strm_.avail_out == 0);
    }

    void Finish() {
        if (finished_) {
            return;
        }

        // Flush any remaining data
        do {
            strm_.avail_out = buffer_.size();
            strm_.next_out = reinterpret_cast<Bytef*>(buffer_.data());

            int ret = deflate(&strm_, Z_FINISH);
            if (ret == Z_STREAM_ERROR) {
                throw std::runtime_error("Zlib compression error");
            }

            size_t have = buffer_.size() - strm_.avail_out;
            if (have > 0) {
                underlying_.Write(buffer_.data(), have);
            }
        } while (strm_.avail_out == 0);

        finished_ = true;
    }

private:
    W& underlying_;

    z_stream strm_{};
    std::vector<char> buffer_;
    bool finished_ = false;
};

}  // namespace mithril::data

#endif
