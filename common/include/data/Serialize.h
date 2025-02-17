#ifndef COMMON_DATA_SERIALIZE_H
#define COMMON_DATA_SERIALIZE_H

#include "data/Writer.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <arpa/inet.h>

namespace mithril::data {

namespace {

template<std::integral T>
constexpr auto hton(T value) {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
        return htons(static_cast<uint16_t>(value));
    } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
        return htonl(static_cast<uint32_t>(value));
    } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return htonll(static_cast<uint64_t>(value));
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type size for network byte order conversion");
    }
}

}  // namespace

/**
 * @brief Serialize defines serializing a type T to a binary format via the
 * Write() function.
 *
 * @tparam T Type to serialize
 */
template<typename T>
struct Serialize;

template<typename T>
concept Serializable = requires(T value, NopWriter writer) {
    { Serialize<T>::Write(value, writer) } -> std::same_as<void>;
};

// All integral types. Serialized to network byte order.
template<std::integral T>
struct Serialize<T> {
    template<Writer W>
    static void Write(T val, W& w) {
        auto ordered = hton(val);
        w.Write(&ordered, sizeof(ordered));
    }
};

// String type. Serialized as [length][N bytes]
template<>
struct Serialize<std::string> {
    template<Writer W>
    static void Write(const std::string& val, W& w) {
        auto length = static_cast<uint32_t>(val.length());
        auto orderedLength = hton(length);
        w.Write(&orderedLength, sizeof(orderedLength));
        w.Write(val.data(), length);
    }
};

// String type. Serialized as [length][N bytes]
template<>
struct Serialize<std::string_view> {
    template<Writer W>
    static void Write(std::string_view val, W& w) {
        auto length = static_cast<uint32_t>(val.length());
        auto orderedLength = hton(length);
        w.Write(&orderedLength, sizeof(orderedLength));
        w.Write(val.data(), length);
    }
};

// Vector of serializable types. Serialized as [length][N x objects]
template<Serializable T>
struct Serialize<std::vector<T>> {
    template<Writer W>
    static void Write(const std::vector<T>& val, W& w) {
        auto size = static_cast<uint32_t>(val.size());
        auto orderedSize = hton(size);
        w.Write(&orderedSize, sizeof(orderedSize));
        for (const auto& e : val) {
            Serialize<T>::Write(e, w);
        }
    }
};

// Vector of strings. Serialized as [N total bytes][NUL separated strings]
template<>
struct Serialize<std::vector<std::string>> {
    template<Writer W>
    static void Write(const std::vector<std::string>& val, W& w) {
        uint32_t totalSize = 0;
        for (const auto& e : val) {
            totalSize += e.size() + 1;
        }
        auto orderedSize = hton(totalSize);
        w.Write(&orderedSize, sizeof(orderedSize));
        for (const auto& e : val) {
            w.Write(e.c_str(), e.size() + 1);
        }
    }
};

// Vector of strings. Serialized as [N total bytes][NUL separated strings]
template<>
struct Serialize<std::vector<std::string_view>> {
    template<Writer W>
    static void Write(const std::vector<std::string_view>& val, W& w) {
        uint32_t totalSize = 0;
        for (const auto& e : val) {
            totalSize += e.size() + 1;
        }

        auto orderedSize = hton(totalSize);
        w.Write(&orderedSize, sizeof(orderedSize));
        for (const auto& e : val) {
            w.Write(e.data(), e.size());
            Serialize<char>::Write('\0', w);
        }
    }
};

/**
 * @brief Serialize a value using a writer.
 *
 * @param val Value to serialize
 * @param writer Writer to serialize to
 */
template<Serializable T, Writer W>
void SerializeValue(const T& val, W& writer) {
    Serialize<T>::Write(val, writer);
}

}  // namespace mithril::data

#endif
