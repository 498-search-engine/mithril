#include "data/Document.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking {
uint32_t GetFinalScore(std::vector<std::string> query, const data::Document& doc, const data::DocInfo& info);
}
