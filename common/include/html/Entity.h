#ifndef COMMON_HTML_ENTITY
#define COMMON_HTML_ENTITY

#include <string>
#include <string_view>

namespace mithril::html {

/**
 * @brief Decodes an HTML entity.
 *
 * @param entity HTML entity to decode. Should start with & and end with ;.
 * @param out Output string to write to
 * @return Whether the HTML entity was valid.
 */
bool DecodeHtmlEntity(std::string_view entity, std::string& out);

/**
 * @brief Decodes a string possibly containing multiple HTML entities.
 *
 * @param s String to decode
 * @return std::string String with all HTML entities decoded.
 */
std::string DecodeHtmlString(std::string_view s);

}  // namespace mithril::html

#endif
