#ifndef CRAWLER_GLOBALS_H
#define CRAWLER_GLOBALS_H

#include <cstddef>
#include <string>
#include <vector>

namespace mithril {

constexpr size_t MaxResponseSize = 2L * 1024L * 1024L;                     // 2 MB
const std::vector<std::string> AllowedMimeTypes = {"text/html"};           // HTML
const std::vector<std::string> AllowedLanguages = {"en", "en-*", "en_*"};  // English

}  // namespace mithril

#endif
