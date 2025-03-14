#ifndef INDEX_TEXTPREPROCESSOR_H
#define INDEX_TEXTPREPROCESSOR_H

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>

namespace mithril {

enum class FieldType {
    BODY = 0,
    TITLE = 1,
    URL = 2,
    ANCHOR = 3
    // Can be extended with HEADING, BOLD, etc.
};

class StopwordFilter {
private:
    static std::unordered_set<std::string> stopwords_;
    static bool initialized_;
    static bool enabled_;

public:
    static void initialize() {
        if (initialized_)
            return;

        stopwords_ = {
            // Basic stopwords
            "a",       "an",      "the",       "and",     "or",      "but",        "is",       "are",        "was",
            "were",    "be",      "been",      "being",   "in",      "on",         "at",       "to",         "for",
            "with",    "by",      "about",     "against", "between", "into",       "through",  "during",     "before",
            "after",   "above",   "below",     "from",    "up",      "down",       "of",       "off",        "over",
            "under",   "again",   "further",   "then",    "once",    "here",       "there",    "when",       "where",
            "why",     "how",     "all",       "any",     "both",    "each",       "few",      "more",       "most",
            "other",   "some",    "such",      "no",      "nor",     "not",        "only",     "own",        "same",
            "so",      "than",    "too",       "very",    "can",     "will",       "just",     "should",     "now",
            "this",    "that",    "these",     "those",   "i",       "me",         "my",       "myself",     "we",
            "our",     "ours",    "ourselves", "you",     "your",    "yours",      "yourself", "yourselves", "he",
            "him",     "his",     "himself",   "she",     "her",     "hers",       "herself",  "it",         "its",
            "itself",  "they",    "them",      "their",   "theirs",  "themselves", "what",     "which",      "who",
            "whom",    "whose",   "as",        "until",   "while",   "because",    "if",       "though",     "unless",
            "whereas", "whether", "although",  "until"};

        initialized_ = true;
        enabled_ = true;
    }

    static bool isStopword(const std::string& term, FieldType field = FieldType::BODY) {
        if (!initialized_)
            initialize();
        if (!enabled_)
            return false;

        bool is_stopword = stopwords_.find(term) != stopwords_.end();
        if (is_stopword && field == FieldType::BODY) {
            return true;
        }

        return false;
    }

    static void setEnabled(bool enabled) {
        if (!initialized_)
            initialize();
        enabled_ = enabled;
    }

    static void addStopword(const std::string& word) {
        if (!initialized_)
            initialize();
        stopwords_.insert(word);
    }

    static void removeStopword(const std::string& word) {
        if (!initialized_)
            initialize();
        stopwords_.erase(word);
    }
};

// init static members
std::unordered_set<std::string> StopwordFilter::stopwords_;
bool StopwordFilter::initialized_ = false;
bool StopwordFilter::enabled_ = true;

class TokenNormalizer {
public:
    static std::string normalize(std::string_view token, FieldType field = FieldType::BODY) {
        if (token.empty())
            return "";

        std::string processed(token);

        // Phase 1 & 2: Content Cleaning & Filtering
        stripHtmlTags(processed);
        removeHtmlEntities(processed);
        smartTrim(processed);
        if (shouldReject(processed))
            return "";

        // Phase 3: Normalization
        smartCaseFold(processed);
        normalizePunctuation(processed);

        if (isValidToken(processed)) {
            if (field == FieldType::BODY && StopwordFilter::isStopword(processed)) {
                return "";  // Filter out stopwords in body text
            }

            if ((field == FieldType::TITLE || field == FieldType::ANCHOR) && StopwordFilter::isStopword(processed) &&
                processed.length() <= 3) {
                return "";  // Only filter very short stopwords in titles
            }

            return decorateToken(processed, field);
        }
        return "";
    }

private:
    static std::string decorateToken(const std::string& token, FieldType field) {
        switch (field) {
        case FieldType::TITLE:
            return "#" + token;
        case FieldType::URL:
            return "@" + token;
        case FieldType::ANCHOR:
            return "$" + token;
        case FieldType::BODY:
        default:
            return token;
        }
    }

    static bool isValidToken(const std::string& str) {
        // Must contain at least one letter and no non-ASCII chars
        return std::any_of(str.begin(), str.end(), ::isalpha) && str.find_first_of("\x80\xFF") == std::string::npos;
    }

    static void smartCaseFold(std::string& str) {
        if (str.length() > 1 && std::all_of(str.begin(), str.end(), ::isupper)) {
            return;  // Preserve valid acronyms
        }
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
    }

    static void stripHtmlTags(std::string& str) {
        bool in_tag = false;
        std::string clean;
        clean.reserve(str.size());

        for (char c : str) {
            if (c == '<' || c == '{')
                in_tag = true;
            else if (c == '>' || c == '}')
                in_tag = false;
            else if (!in_tag)
                clean += c;
        }
        str.swap(clean);
    }

    static void removeHtmlEntities(std::string& str) {
        size_t pos = 0;
        while ((pos = str.find('&', pos)) != std::string::npos) {
            size_t end = str.find(';', pos);
            if (end != std::string::npos) {
                str.erase(pos, end - pos + 1);
            } else
                break;
        }
    }

    static void smartTrim(std::string& str) {
        constexpr std::string_view TRIM_CHARS = " \t\n\r\f\v!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

        size_t first = str.find_first_not_of(TRIM_CHARS);
        if (first == std::string::npos)
            str.clear();
        else
            str = str.substr(first, str.find_last_not_of(TRIM_CHARS) - first + 1);
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
        if (str.empty())
            return true;

        // Reject pure numbers but allow alphanumeric
        bool has_letter = false;
        bool all_digits = true;

        for (char c : str) {
            if (std::isalpha(c)) {
                has_letter = true;
                all_digits = false;
            } else if (!std::isdigit(c)) {
                all_digits = false;
            }
        }

        // Reject pure numbers, keep alphanumeric
        if (all_digits)
            return true;

        // Still reject URLs and other patterns
        const bool has_bad_pattern = str.find("//") != std::string::npos || str.find('|') != std::string::npos ||
                                     str.find('=') != std::string::npos || str.find("www.") != std::string::npos ||
                                     str.find(".com") != std::string::npos;

        return has_bad_pattern || str.length() > 64;
    }
};

}  // namespace mithril

#endif