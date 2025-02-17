#ifndef INDEX_TEXTPREPROCESSOR_H
#define INDEX_TEXTPREPROCESSOR_H

#include <cctype>
#include <string>
#include <string_view>

namespace mithril {

class TokenNormalizer {
public:
    static std::string normalize(std::string_view token) {
        // early reject obvious cases
        if (token.empty() || isAllPunctuation(token) || isAllDigits(token) || containsCJK(token)) {
            return "";
        }

        // Normalize the token
        std::string normalized = trim(token);

        // Must contain at least one English letter
        if (!containsEnglishLetter(normalized)) {
            return "";
        }

        // Remove HTML entities
        if (normalized.find('&') != std::string::npos) {
            removeHtmlEntities(normalized);
        }

        // Normalize internal punctuation (e.g., multiple spaces/dots)
        normalizeInternalPunctuation(normalized);

        return normalized;
    }

private:
    static bool isAllPunctuation(std::string_view token) {
        return std::all_of(token.begin(), token.end(), [](char c) { return std::ispunct(c); });
    }

    static bool isAllDigits(std::string_view token) {
        return std::all_of(token.begin(), token.end(), [](char c) { return std::isdigit(c); });
    }

    static bool containsCJK(std::string_view token) {
        return std::any_of(token.begin(), token.end(), [](char c) { return static_cast<unsigned char>(c) > 127; });
    }

    static bool containsEnglishLetter(std::string_view token) {
        return std::any_of(token.begin(), token.end(), [](char c) { return std::isalpha(c); });
    }

    static void removeHtmlEntities(std::string& token) {
        size_t pos;
        while ((pos = token.find('&')) != std::string::npos) {
            size_t end = token.find(';', pos);
            if (end != std::string::npos) {
                token.erase(pos, end - pos + 1);
            } else
                break;
        }
    }

    static void normalizeInternalPunctuation(std::string& token) {
        // Replace multiple spaces with single space
        auto new_end =
            std::unique(token.begin(), token.end(), [](char a, char b) { return a == b && std::isspace(a); });
        token.erase(new_end, token.end());

        // Replace multiple dots/punctuation with single
        new_end = std::unique(token.begin(), token.end(), [](char a, char b) { return a == b && std::ispunct(a); });
        token.erase(new_end, token.end());
    }

    static std::string trim(std::string_view token) {
        auto start = token.find_first_not_of(" \t\n\r\f\v.,!?;:\"'");
        if (start == std::string::npos)
            return "";

        auto end = token.find_last_not_of(" \t\n\r\f\v.,!?;:\"'");
        return std::string(token.substr(start, end - start + 1));
    }
};

}  // namespace mithril

#endif