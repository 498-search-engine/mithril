#ifndef COMMON_HTML_LINK_H
#define COMMON_HTML_LINK_H

#include "http/URL.h"

#include <optional>
#include <string>
#include <string_view>

namespace mithril::html {

std::optional<std::string> MakeAbsoluteLink(const http::URL& currentUrl, std::string_view base, std::string_view href);

}  // namespace mithril::html

#endif
