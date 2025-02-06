#ifndef COMMON_HTML_LINK_H
#define COMMON_HTML_LINK_H

#include "http/URL.h"

#include <optional>
#include <string>

namespace mithril::html {

std::optional<std::string>
MakeAbsoluteLink(const http::URL& currentUrl, const std::string& base, const std::string& href);

}  // namespace mithril::html

#endif
