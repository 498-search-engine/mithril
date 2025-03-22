#include "html/Entity.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mithril::html {

namespace {

const std::unordered_map<std::string_view, std::string_view> NamedEntities = {
    // Common entities
    {    "amp",   "&"},
    {     "lt",   "<"},
    {     "gt",   ">"},
    {   "quot",  "\""},
    {   "apos",   "'"},
    {   "nbsp",   " "},
    {   "copy",  "©"},
    {    "reg",  "®"},
    {    "deg",  "°"},

    // Arrows
    {   "larr", "←"},
    {   "rarr", "→"},
    {   "uarr", "↑"},
    {   "darr", "↓"},
    {   "harr", "↔"},
    {   "lArr", "⇐"},
    {   "rArr", "⇒"},
    {   "uArr", "⇑"},
    {   "dArr", "⇓"},
    {   "hArr", "⇔"},
    {  "crarr", "↵"},
    { "lsaquo", "‹"},
    { "rsaquo", "›"},
    {  "laquo",  "«"},
    {  "raquo",  "»"},

    // Dots/Points
    { "middot",  "·"},
    {   "bull", "•"},
    { "hellip", "…"},
    {  "prime", "′"},
    {  "Prime", "″"},
    {   "sdot", "⋅"},

    // Greek Letters (lowercase)
    {  "alpha",  "α"},
    {   "beta",  "β"},
    {  "gamma",  "γ"},
    {  "delta",  "δ"},
    {"epsilon",  "ε"},
    {   "zeta",  "ζ"},
    {    "eta",  "η"},
    {  "theta",  "θ"},
    {   "iota",  "ι"},
    {  "kappa",  "κ"},
    { "lambda",  "λ"},
    {     "mu",  "μ"},
    {     "nu",  "ν"},
    {     "xi",  "ξ"},
    {"omicron",  "ο"},
    {     "pi",  "π"},
    {    "rho",  "ρ"},
    {  "sigma",  "σ"},
    {    "tau",  "τ"},
    {"upsilon",  "υ"},
    {    "phi",  "φ"},
    {    "chi",  "χ"},
    {    "psi",  "ψ"},
    {  "omega",  "ω"},

    // Greek Letters (uppercase)
    {  "Gamma",  "Γ"},
    {  "Delta",  "Δ"},
    {  "Theta",  "Θ"},
    { "Lambda",  "Λ"},
    {     "Xi",  "Ξ"},
    {     "Pi",  "Π"},
    {  "Sigma",  "Σ"},
    {    "Phi",  "Φ"},
    {    "Psi",  "Ψ"},
    {  "Omega",  "Ω"},

    // Mathematical Symbols
    {  "minus", "−"},
    { "plusmn",  "±"},
    {  "times",  "×"},
    { "divide",  "÷"},
    {  "frasl", "⁄"},
    {    "sum", "∑"},
    {   "prod", "∏"},
    {    "not",  "¬"},
    {   "part", "∂"},
    { "forall", "∀"},
    {  "exist", "∃"},
    {  "empty", "∅"},
    {   "isin", "∈"},
    {  "notin", "∉"},
    {     "ni", "∋"},
    {  "nabla", "∇"},
    {   "prop", "∝"},
    {  "infin", "∞"},
    {    "ang", "∠"},
    {  "asymp", "≈"},
    {     "ne", "≠"},
    {  "equiv", "≡"},
    {     "le", "≤"},
    {     "ge", "≥"},
    {    "sub", "⊂"},
    {    "sup", "⊃"},
    {   "nsub", "⊄"},
    {   "sube", "⊆"},
    {   "supe", "⊇"},
    {    "int", "∫"},
    {  "radic", "√"},
    {  "lceil", "⌈"},
    {  "rceil", "⌉"},
    { "lfloor", "⌊"},
    { "rfloor", "⌋"},

    // Currency Symbols
    { "dollar",   "$"},
    { "curren",  "¤"},
    {   "euro", "€"},
    {  "pound",  "£"},
    {    "yen",  "¥"},
    {   "cent",  "¢"},

    // Other Useful Symbols
    {  "trade", "™"},
    { "permil", "‰"},
    {    "loz", "◊"},
    { "spades", "♠"},
    {  "clubs", "♣"},
    { "hearts", "♥"},
    {  "diams", "♦"},
    {   "sect",  "§"},
    {   "para",  "¶"},
    { "dagger", "†"},
    { "Dagger", "‡"},
    {   "ensp",   " "}, // en space
    {   "emsp",   " "}, // em space
    { "thinsp",   " "}, // thin space
    {  "ndash", "–"},
    {  "mdash", "—"},
    {  "sbquo", "‚"},
    {  "bdquo", "„"},
    {  "ldquo",  "\""},
    {  "rdquo",  "\""},
    {  "lsquo",   "'"},
    {  "rsquo",   "'"},
    {  "tilde",  "˜"},
    {   "circ",  "ˆ"},
    { "brvbar",  "¦"},
    { "frac14",  "¼"},
    { "frac12",  "½"},
    { "frac34",  "¾"},
    { "iquest",  "¿"},
    {  "iexcl",  "¡"},
    {  "micro",  "µ"},
};

bool DecodeNamedEntity(std::string_view content, std::string& out) {
    auto it = NamedEntities.find(content);
    if (it != NamedEntities.end()) {
        out.append(it->second);
        return true;
    }
    return false;
}

bool DecodeNumericEntity(std::string_view content, std::string& out) {
    // Remove the # character
    content = content.substr(1);

    if (content.empty()) {
        return false;
    }

    unsigned int codePoint;

    if (content[0] == 'x' || content[0] == 'X') {
        // Handle hexadecimal entities (&#xhhhh;)
        content = content.substr(1);

        // Check if content is not empty and consists of valid hex digits
        if (content.empty() || !std::all_of(content.begin(), content.end(), [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c));
            })) {
            return false;
        }

        // Convert hexadecimal to integer
        try {
            codePoint = std::stoul(std::string(content), nullptr, 16);
        } catch (const std::invalid_argument&) {
            return false;
        } catch (const std::out_of_range&) {
            return false;
        }
    } else {
        // Handle decimal entities (&#dddd;)
        // Check if content consists of valid decimal digits
        if (!std::all_of(
                content.begin(), content.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
            return false;
        }

        // Convert decimal to integer
        try {
            codePoint = std::stoul(std::string(content));
        } catch (const std::invalid_argument&) {
            return false;
        } catch (const std::out_of_range&) {
            return false;
        }
    }

    if (codePoint == 0xA0) {
        // NBSP https://www.codetable.net/hex/A0
        out.push_back(' ');
        return true;
    }

    // Encode the code point as UTF-8 and append to the output
    if (codePoint <= 0x7F) {  // ASCII range
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        return false;  // Invalid Unicode code point
    }

    return true;
}

}  // namespace

bool DecodeHtmlEntity(std::string_view entity, std::string& out) {
    // Check if entity has the minimum required length (at least &x;)
    if (entity.length() < 3) {
        return false;
    }

    // Check if entity starts with & and ends with ;
    if (entity[0] != '&' || entity[entity.length() - 1] != ';') {
        return false;
    }

    // Extract the content between & and ;
    std::string_view content = entity.substr(1, entity.length() - 2);

    if (content.empty()) {
        return false;
    }

    if (content[0] == '#') {
        return DecodeNumericEntity(content, out);
    } else {
        return DecodeNamedEntity(content, out);
    }
}

std::string DecodeHtmlString(std::string_view s) {
    std::string decoded;
    size_t start = 0;

    while (start < s.size()) {
        auto entityStart = s.find('&', start);
        if (entityStart == std::string_view::npos) {
            break;
        }

        auto entityEnd = s.find(';', entityStart);
        if (entityEnd == std::string_view::npos) {
            break;
        }

        auto entity = s.substr(entityStart, entityEnd - entityStart + 1);

        // Add text before start of entity
        decoded.append(s.substr(start, entityStart - start));

        bool ok = DecodeHtmlEntity(entity, decoded);
        if (!ok) {
            // Entity wasn't valid, treat it as normal text
            decoded.append(entity);
        }

        start = entityEnd + 1;
    }

    if (start < s.size()) {
        // Add any text after last entity
        decoded.append(s.substr(start));
    }

    return decoded;
}

}  // namespace mithril::html
