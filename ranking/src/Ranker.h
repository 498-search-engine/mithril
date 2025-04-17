#include "data/Document.h"

#include <spdlog/spdlog.h>

namespace mithril::ranking {
uint32_t GetFinalScore(const data::Document& doc, const data::DocInfo& info);
}
