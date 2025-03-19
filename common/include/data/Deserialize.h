#ifndef COMMON_DATA_DESERIALIZE_H
#define COMMON_DATA_DESERIALIZE_H

#include "data/Reader.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <arpa/inet.h>

#ifdef __APPLE__
// On macOS, ntohll might be available or we define it
#    ifndef ntohll
#        define ntohll(x) (((uint64_t)ntohl((uint32_t)((x) >> 32))) | (((uint64_t)ntohl((uint32_t)(x))) << 32))
#    endif
#else
// Linux and other platforms
#    include <endian.h>
// Use be64toh as equivalent to ntohll
#    define ntohll(x) be64toh(x)
#endif

namespace mithril::data {

namespace {

template<std::integral T>
constexpr auto ntoh(T value) {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
        return ntohs(static_cast<uint16_t>(value));
    } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
        return ntohl(static_cast<uint32_t>(value));
    } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return ntohll(static_cast<uint64_t>(value));
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type size for network byte order conversion");
    }
}

}  // namespace

template<typename T>
struct Deserialize;

template<typename T>
concept Deserializable = requires(T value, NopReader reader) {
    { Deserialize<T>::Read(value, reader) } -> std::same_as<bool>;
};

template<std::integral T>
struct Deserialize<T> {
    template<Reader R>
    static bool Read(T& value, R& reader) {
        T unordered;
        if (!reader.Read(&unordered, sizeof(T))) {
            return false;
        }
        value = ntoh(unordered);
        return true;
    }
};

template<>
struct Deserialize<std::string> {
    template<Reader R>
    static bool Read(std::string& value, R& reader) {
        uint32_t length;
        if (!Deserialize<uint32_t>::Read(length, reader)) {
            return false;
        }
        value.clear();
        value.resize(length);
        return reader.Read(value.data(), length);
    }
};

template<Deserializable T>
struct Deserialize<std::vector<T>> {
    template<Reader R>
    static bool Read(std::vector<T>& val, R& reader) {
        uint32_t length;
        if (!Deserialize<uint32_t>::Read(length, reader)) {
            return false;
        }

        val.clear();
        val.reserve(length);

        for (uint32_t i = 0; i < length; ++i) {
            T element;
            if (!Deserialize<T>::Read(element, reader)) {
                return false;
            }
            val.push_back(std::move(element));
        }

        return true;
    }
};

template<>
struct Deserialize<std::vector<std::string>> {
    template<Reader R>
    static bool Read(std::vector<std::string>& val, R& reader) {
        uint32_t length;
        if (!Deserialize<uint32_t>::Read(length, reader)) {
            return false;
        }

        if (length == 0) {
            val.clear();
            return true;
        }

        std::vector<char> raw;
        raw.resize(length);
        if (!reader.Read(raw.data(), length)) {
            return false;
        }

        val.clear();

        // Find strings by boundary
        const char* c = raw.data();
        const char* stringStart = c;
        const char* end = raw.data() + raw.size();
        while (c < end) {
            // Find null byte
            while (c < end && *c != '\0') {
                ++c;
            }
            // Use string_view constructor as we know the length of the string
            size_t len = c - stringStart;
            val.emplace_back(std::string_view{stringStart, len});

            ++c;  // Consume null byte
            stringStart = c;
        }

        return true;
    }
};

/**
 * @brief Deserialize a value from a reader.
 *
 * @param value Value reference to deserialize into
 * @param reader Reader to deserialize from
 * @return true Success
 * @return false Failure
 */
template<typename T, Reader R>
bool DeserializeValue(T& value, R& reader) {
    return Deserialize<T>::Read(value, reader);
}

}  // namespace mithril::data

#endif
