#include "Util.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

bool InsensitiveCharEquals(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

bool InsensitiveStrEquals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), InsensitiveCharEquals);
}

bool InsensitiveStartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin(), InsensitiveCharEquals);
}

size_t FindCaseInsensitive(const std::string& s, const char* q) {
    auto len = std::strlen(q);
    auto it = std::search(s.begin(), s.end(), q, q + len, [](unsigned char a, unsigned char b) -> bool {
        return std::tolower(a) == std::tolower(b);
    });
    if (it == s.end()) {
        return std::string::npos;
    } else {
        return std::distance(s.begin(), it);
    }
}

std::string ToLowerCase(std::string_view s) {
    auto r = std::string{s};
    for (auto& c : r) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return r;
}

std::vector<std::string_view> SplitString(std::string_view s, char c) {
    std::vector<std::string_view> res;
    size_t pos = 0;
    while (pos < s.size()) {
        auto lineEnd = s.find(c, pos);
        if (lineEnd == std::string::npos) {
            lineEnd = s.size();
        }

        auto line = s.substr(pos, lineEnd - pos);
        res.push_back(line);
        pos = lineEnd + 1;
    }
    return res;
}

std::string ReadFile(const char* filepath) {
    // Open file in binary mode
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        throw std::runtime_error("failed to open file " + std::string{filepath});
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);

    std::string buffer;
    buffer.resize(size);

    // Read file from beginning
    fseek(file, 0, SEEK_SET);
    size_t bytesRead = fread(buffer.data(), 1, size, file);
    fclose(file);

    // Check if we read the expected number of bytes
    if (bytesRead != size) {
        throw std::runtime_error("failed to read file " + std::string{filepath});
    }

    return buffer;
}

std::vector<std::string_view> GetLines(std::string_view data) {
    return SplitString(data, '\n');
}
