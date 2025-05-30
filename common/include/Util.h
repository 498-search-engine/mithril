#ifndef COMMON_UTIL_H
#define COMMON_UTIL_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

bool InsensitiveCharEquals(char a, char b);

bool InsensitiveStrEquals(std::string_view a, std::string_view b);

bool InsensitiveStartsWith(std::string_view s, std::string_view prefix);

size_t FindCaseInsensitive(const std::string& s, const char* q);

std::string ToLowerCase(std::string_view s);

constexpr std::vector<std::string_view> SplitPath(std::string_view path) {
    std::vector<std::string_view> segments;
    size_t start = 0;

    // Skip leading slash
    if (!path.empty() && path[0] == '/') {
        start = 1;
    }

    while (true) {
        size_t end = path.find('/', start);
        if (end == std::string_view::npos) {
            segments.push_back(path.substr(start));
            break;
        }
        segments.push_back(path.substr(start, end - start));
        start = end + 1;
    }

    return segments;
}

static_assert(SplitPath("/hello/world/123/")[0] == "hello");
static_assert(SplitPath("/hello/world/123/")[1] == "world");
static_assert(SplitPath("/hello/world/123/")[2] == "123");
static_assert(SplitPath("/hello/world/123/")[3] == "");

constexpr std::string JoinPath(const std::vector<std::string_view>& segments) {
    std::string result;
    for (const auto& segment : segments) {
        result.push_back('/');
        result.append(segment);
    }
    return result;
}

static_assert(JoinPath(SplitPath("/hello/world/123/")) == "/hello/world/123/");
static_assert(JoinPath(SplitPath("/hello/world/123")) == "/hello/world/123");

constexpr std::string ResolvePath(std::string_view path) {
    auto segments = SplitPath(path);
    std::vector<std::string_view> resolved;

    for (const auto& segment : segments) {
        if (segment == ".." && !resolved.empty()) {
            resolved.pop_back();
        } else if (segment != ".") {
            resolved.push_back(segment);
        }
    }

    return JoinPath(resolved);
}

static_assert(ResolvePath("/a/b/./c/d/../e/f") == "/a/b/c/e/f");
static_assert(ResolvePath("/a/../../../c") == "/c");
static_assert(ResolvePath("/a/./././.") == "/a");
static_assert(ResolvePath("/a/././././") == "/a/");

template<typename F>
std::vector<std::string_view> SplitStringOn(std::string_view s, F f) {
    std::vector<std::string_view> res;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t lineEnd;
        auto it = std::find_if(s.begin() + pos, s.end(), f);
        if (it == s.end()) {
            lineEnd = s.size();
        } else {
            lineEnd = it - s.begin();
        }

        auto line = s.substr(pos, lineEnd - pos);
        res.push_back(line);
        pos = lineEnd + 1;
    }
    return res;
}

std::vector<std::string_view> SplitString(std::string_view s, char c);

std::string ReadFile(const char* filepath);

std::vector<std::string_view> GetLines(std::string_view data);

std::vector<std::string_view> GetCommaSeparatedList(std::string_view s);

std::vector<std::string_view> GetWords(std::string_view s);

#endif
