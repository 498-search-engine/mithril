#ifndef INDEX_UTILS_H
#define INDEX_UTILS_H

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace mithril {

class VByteCodec {
public:
    static void encode(uint32_t value, std::ostream& out) {
        while (value >= 128) {
            out.put((value & 127) | 128);
            if (!out)
                throw std::runtime_error("Failed to write VByte");
            value >>= 7;
        }
        out.put(value);
        if (!out)
            throw std::runtime_error("Failed to write VByte");
    }

    static uint32_t decode(std::istream& in) {
        uint32_t result = 0;
        uint32_t shift = 0;
        uint8_t byte;
        do {
            byte = in.get();
            result |= (byte & 127) << shift;
            shift += 7;
        } while (byte & 128);
        return result;
    }

    static uint32_t decode_from_memory(const char*& buffer) {
        uint32_t result = 0;
        uint32_t shift = 0;
        uint8_t byte;

        do {
            byte = *reinterpret_cast<const uint8_t*>(buffer++);
            result |= (byte & 127) << shift;
            shift += 7;
        } while (byte & 128);

        return result;
    }

    static void encode_to_memory(uint32_t value, char*& buffer, size_t& remaining_space) {
        while (value >= 128) {
            if (remaining_space < 1)
                throw std::runtime_error("Buffer overflow in VByte encoding");
            *buffer++ = (value & 127) | 128;
            remaining_space--;
            value >>= 7;
        }

        if (remaining_space < 1)
            throw std::runtime_error("Buffer overflow in VByte encoding");
        *buffer++ = value;
        remaining_space--;
    }

    static size_t max_bytes_needed(uint32_t value) {
        if (value < 128)
            return 1;
        if (value < 16384)
            return 2;
        if (value < 2097152)
            return 3;
        if (value < 268435456)
            return 4;
        return 5;
    }

    static void encodeBatch(const std::vector<uint32_t>& deltas, std::ostream& out) {
        const size_t BATCH_SIZE = 1024;
        char buffer[BATCH_SIZE];
        char* ptr = buffer;
        size_t remaining = BATCH_SIZE;

        for (uint32_t delta : deltas) {
            // Check if we need to flush the buffer
            if (remaining < 5) {  // Max 5 bytes per VByte
                out.write(buffer, BATCH_SIZE - remaining);
                ptr = buffer;
                remaining = BATCH_SIZE;
            }

            // Encode delta into buffer
            size_t before = remaining;
            encode_to_memory(delta, ptr, remaining);
            if (!out.good()) {
                throw std::runtime_error("Failed to write VByte batch");
            }
        }

        // Write any remaining data
        if (remaining < BATCH_SIZE) {
            out.write(buffer, BATCH_SIZE - remaining);
        }
    }
};


}  // namespace mithril

#endif  // INDEX_UTILS_H
