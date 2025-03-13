#include "Util.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>

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
