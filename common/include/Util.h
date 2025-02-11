#ifndef COMMON_UTIL_H
#define COMMON_UTIL_H

#include <string>
#include <string_view>
#include <vector>

bool InsensitiveCharEquals(char a, char b);

bool InsensitiveStrEquals(std::string_view a, std::string_view b);

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

#endif
