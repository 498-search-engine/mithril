#ifndef INDEX_TEXTPREPROCESSOR_H
#define INDEX_TEXTPREPROCESSOR_H

#include <cctype>
#include <string>
#include <string_view>
#include <algorithm>

namespace mithril {

class TokenNormalizer {
public:
    static std::string normalize(std::string_view token) {
        if (token.empty()) return "";

        std::string processed(token);
        
        // Phase 1: Content Cleaning
        stripHtmlTags(processed);
        removeHtmlEntities(processed);
        smartTrim(processed);
        
        // Phase 2: Aggressive Filtering
        if (shouldReject(processed)) return "";

        // Phase 3: Normalization
        smartCaseFold(processed);
        normalizePunctuation(processed);

        // Final validation
        return isValidToken(processed) ? processed : "";
    }

private:
    static bool isValidToken(const std::string& str) {
        // Must contain at least one letter and no non-ASCII chars
        return std::any_of(str.begin(), str.end(), ::isalpha) &&
               str.find_first_of("\x80\xFF") == std::string::npos;
    }

    static void smartCaseFold(std::string& str) {
        if (str.length() > 1 && 
            std::all_of(str.begin(), str.end(), ::isupper)) {
            return;  // Preserve valid acronyms
        }
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c){ return std::tolower(c); });
    }

    static void stripHtmlTags(std::string& str) {
        bool in_tag = false;
        std::string clean;
        clean.reserve(str.size());

        for (char c : str) {
            if (c == '<' || c == '{') in_tag = true;
            else if (c == '>' || c == '}') in_tag = false;
            else if (!in_tag) clean += c;
        }
        str.swap(clean);
    }

    static void removeHtmlEntities(std::string& str) {
        size_t pos = 0;
        while ((pos = str.find('&', pos)) != std::string::npos) {
            size_t end = str.find(';', pos);
            if (end != std::string::npos) {
                str.erase(pos, end - pos + 1);
            } else break;
        }
    }

    static void smartTrim(std::string& str) {
        constexpr std::string_view TRIM_CHARS = 
            " \t\n\r\f\v!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
        
        size_t first = str.find_first_not_of(TRIM_CHARS);
        if (first == std::string::npos) str.clear();
        else str = str.substr(first, str.find_last_not_of(TRIM_CHARS) - first + 1);
    }

    static void normalizePunctuation(std::string& str) {
        std::string clean;
        bool prev_punct = false;
        
        for (char c : str) {
            if (std::ispunct(c)) {
                if (!prev_punct && !clean.empty()) {
                    clean += ' ';
                    prev_punct = true;
                }
            } else {
                clean += c;
                prev_punct = false;
            }
        }
        str.swap(clean);
    }

    static bool shouldReject(const std::string& str) {
        if (str.empty()) return true;

        // Absolute numeric rejection
        if (std::any_of(str.begin(), str.end(), ::isdigit)) return true;

        // Structural rejection patterns
        const bool has_bad_pattern =
            str.find("//") != std::string::npos ||
            str.find('|') != std::string::npos ||
            str.find('=') != std::string::npos ||
            str.find("www.") != std::string::npos ||
            str.find(".com") != std::string::npos;

        return has_bad_pattern || str.length() > 64;
    }
};

} // namespace mithril

#endif