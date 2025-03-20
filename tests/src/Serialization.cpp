#include "data/Deserialize.h"
#include "data/Reader.h"
#include "data/Serialize.h"
#include "data/Writer.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <gtest/gtest.h>

using namespace mithril::data;

class Serialization : public ::testing::Test {
protected:
    template<typename T>
    T RoundTrip(const T& value) {
        // Serialize
        BufferWriter writer;
        SerializeValue(value, writer);
        auto buffer = writer.Release();

        // Deserialize
        BufferReader reader(buffer);
        T result;
        EXPECT_TRUE(DeserializeValue(result, reader));
        return result;
    }

    template<typename T>
    void ExpectRoundTrip(const T& value) {
        EXPECT_EQ(RoundTrip(value), value);
    }
};

TEST_F(Serialization, BasicTypes) {
    ExpectRoundTrip(true);
    ExpectRoundTrip(false);
    ExpectRoundTrip('a');
    ExpectRoundTrip('\0');
    ExpectRoundTrip(uint8_t{42});
    ExpectRoundTrip(int8_t{-42});
    ExpectRoundTrip(uint16_t{12345});
    ExpectRoundTrip(int16_t{-12345});
    ExpectRoundTrip(uint32_t{305419896});
    ExpectRoundTrip(int32_t{-305419896});
    ExpectRoundTrip(uint64_t{1234567890123456789ULL});
    ExpectRoundTrip(int64_t{-1234567890123456789LL});
}

TEST_F(Serialization, Strings) {
    ExpectRoundTrip(std::string{});
    ExpectRoundTrip(std::string{"Hello, World!"});
    ExpectRoundTrip(std::string(1000, 'x'));  // Long string

    // String with embedded nulls
    ExpectRoundTrip(std::string{"Hello\0World", 11});
}

TEST_F(Serialization, StringView) {
    const char* text = "Hello, World!";
    std::string_view view(text);

    BufferWriter writer;
    SerializeValue(view, writer);
    auto buffer = writer.Release();

    std::string result;
    BufferReader reader(buffer);
    EXPECT_TRUE(DeserializeValue(result, reader));
    EXPECT_EQ(result, view);
}

TEST_F(Serialization, Vectors) {
    ExpectRoundTrip(std::vector<int>{});
    ExpectRoundTrip(std::vector<int>{1, 2, 3, 4, 5});
    ExpectRoundTrip(std::vector<std::string>{"hello", "world"});

    // Nested vectors
    ExpectRoundTrip(std::vector<std::vector<int>>{
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    });
}

TEST_F(Serialization, ErrorCases) {
    // Write some data
    BufferWriter writer;
    SerializeValue(uint32_t{0x12345678}, writer);
    auto buffer = writer.Release();

    // Try to read with insufficient buffer size
    BufferReader reader(std::span<const char>(buffer.data(), buffer.size() - 1));
    uint32_t value;
    EXPECT_FALSE(DeserializeValue(value, reader));
}

struct Person {
    std::string name;
    uint32_t age{};
    std::vector<std::string> hobbies;

    bool operator==(const Person& other) const {
        return name == other.name && age == other.age && hobbies == other.hobbies;
    }
};

namespace mithril::data {

template<>
struct Serialize<Person> {
    template<Writer W>
    static void Write(const Person& val, W& w) {
        SerializeValue(val.name, w);
        SerializeValue(val.age, w);
        SerializeValue(val.hobbies, w);
    }
};

template<>
struct Deserialize<Person> {
    template<Reader R>
    static bool Read(Person& val, R& r) {
        return DeserializeValue(val.name, r) && DeserializeValue(val.age, r) && DeserializeValue(val.hobbies, r);
    }
};

}  // namespace mithril::data

TEST_F(Serialization, ComplexStructure) {
    Person person{
        .name = "John Doe",
        .age = 30,
        .hobbies = {"reading", "hiking", "coding"},
    };
    ExpectRoundTrip(person);
}
