#include "data/Gzip.h"

#include "core/array.h"

#include <stdexcept>
#include <vector>
#include <zconf.h>
#include <zlib.h>

namespace mithril::data {

std::vector<char> Gunzip(const std::vector<char>& compressed) {
    if (compressed.empty()) {
        return {};
    }

    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = 0;
    zs.next_in = Z_NULL;

    if (inflateInit2(&zs, 31) != Z_OK) {  // 15 + 16 for gzip format
        throw std::runtime_error("Failed to initialize zlib");
    }

    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const unsigned char*>(compressed.data()));
    zs.avail_in = static_cast<uInt>(compressed.size());

    // Create output vector with some reserved space
    std::vector<char> decompressed;
    decompressed.reserve(compressed.size() * 4);

    core::Array<unsigned char, GzipChunkSize> buf{};
    int ret;
    do {
        zs.next_out = buf.Data();
        zs.avail_out = static_cast<uInt>(buf.Size());

        // Decompress
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            throw std::runtime_error("Zlib compression error");
        }

        auto amount = buf.Size() - zs.avail_out;
        decompressed.insert(decompressed.end(), buf.begin(), buf.begin() + amount);
    } while (ret != Z_STREAM_END);

    // Clean up
    inflateEnd(&zs);

    return decompressed;
}

}  // namespace mithril::data
