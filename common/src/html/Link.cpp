#include "html/Link.h"

#include <string>
#include <string_view>
#include <vector>

namespace mithril::html {

namespace {

constexpr std::vector<std::string_view> SplitPath(std::string_view path) {
    std::vector<std::string_view> segments;
    if (path.empty()) {
        return segments;
    }

    size_t start = 0;
    if (path[0] == '/') {
        ++start;
    }

    while (true) {
        auto end = path.find('/', start);
        if (end == std::string_view::npos) {
            segments.push_back(path.substr(start, path.size() - start));
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

}  // namespace

std::optional<std::string> MakeAbsoluteLink(const http::URL& currentUrl, std::string_view base, std::string_view href) {
    // If href is empty, return nullopt
    if (href.empty()) {
        return std::nullopt;
    }

    // Skip non-crawlable URLs
    const std::vector<std::string> nonCrawlable = {
        "javascript:", "data:", "mailto:", "tel:", "sms:", "ftp:", "#", "about:", "file:", "ws:", "wss:"};

    for (const auto& prefix : nonCrawlable) {
        if (href.substr(0, prefix.length()) == prefix) {
            return std::nullopt;
        }
    }

    // If href is already absolute URL, return it
    if (href.substr(0, 7) == "http://" || href.substr(0, 8) == "https://") {
        return std::string{href};
    }

    // Handle protocol-relative URLs
    if (href.substr(0, 2) == "//") {
        return currentUrl.scheme + ":" + std::string{href};
    }

    // Handle root-relative URLs
    if (href[0] == '/') {
        std::string portPart = currentUrl.port.empty() ? "" : ":" + currentUrl.port;
        return currentUrl.scheme + "://" + currentUrl.host + portPart + ResolvePath(href);
    }

    // Handle relative URLs
    std::string basePath;
    if (!base.empty()) {
        // If base tag is present, use it
        if (base[0] == '/') {
            basePath = base;
        } else if (base.substr(0, 7) == "http://" || base.substr(0, 8) == "https://") {
            // If base is absolute, extract its path
            size_t pathStart = base.find('/', base.find("//") + 2);
            if (pathStart != std::string::npos) {
                basePath = base.substr(pathStart);
            }
        } else {
            basePath = "/" + std::string{base};
        }
    } else {
        // Use the current URL's path as base
        basePath = currentUrl.path;
    }

    // Remove filename from basePath if it exists
    size_t lastSlash = basePath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        basePath = basePath.substr(0, lastSlash + 1);
    }

    // Combine base path with href and resolve
    auto resolvedPath = ResolvePath(basePath + std::string{href});
    auto portPart = currentUrl.port.empty() ? "" : ":" + currentUrl.port;
    return currentUrl.scheme + "://" + currentUrl.host + portPart + resolvedPath;
}

}  // namespace mithril::html
