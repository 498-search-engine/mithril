#ifndef INDEX_TEXTPREPROCESSOR_H
#define INDEX_TEXTPREPROCESSOR_H

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace mithril {

enum class FieldType {
    BODY = 0,
    TITLE = 1,
    URL = 2,
    ANCHOR = 3,
    DESC = 4
    // Can be extended with HEADING, BOLD, etc.
};

constexpr uint8_t FIELD_FLAG_BODY = 1 << 0;
constexpr uint8_t FIELD_FLAG_TITLE = 1 << 1;
constexpr uint8_t FIELD_FLAG_URL = 1 << 2;
constexpr uint8_t FIELD_FLAG_ANCHOR = 1 << 3;
constexpr uint8_t FIELD_FLAG_DESC = 1 << 4;

inline uint8_t fieldTypeToFlag(FieldType type) {
    return 1 << static_cast<uint8_t>(type);
}

class StopwordFilter {
private:
    inline static std::once_flag init_flag_;
    inline static std::unordered_set<std::string> stopwords_;
    inline static bool enabled_ = true;

    static void initialize_internal() {
        stopwords_ = {// Articles & Determiners
                      "a",
                      "an",
                      "the",
                      "this",
                      "that",
                      "these",
                      "those",
                      "some",
                      "any",
                      "all",
                      "no",
                      "every",
                      "each",
                      "few",
                      "more",
                      "most",
                      "other",
                      "such",
                      "what",
                      "which",
                      "whose",
                      // Pronouns
                      "i",
                      "me",
                      "my",
                      "mine",
                      "myself",
                      "you",
                      "your",
                      "yours",
                      "yourself",
                      "yourselves",
                      "he",
                      "him",
                      "his",
                      "himself",
                      "she",
                      "her",
                      "hers",
                      "herself",
                      "it",
                      "its",
                      "itself",
                      "we",
                      "us",
                      "our",
                      "ours",
                      "ourselves",
                      "they",
                      "them",
                      "their",
                      "theirs",
                      "themselves",
                      // Prepositions & Conjunctions & Common Verbs/Adverbs etc.
                      "about",
                      "above",
                      "across",
                      "after",
                      "afterwards",
                      "again",
                      "against",
                      "along",
                      "already",
                      "also",
                      "although",
                      "always",
                      "am",
                      "among",
                      "amongst",
                      "amount",
                      "and",
                      "another",
                      "anyhow",
                      "anyone",
                      "anything",
                      "anyway",
                      "anywhere",
                      "are",
                      "around",
                      "as",
                      "at",
                      "back",
                      "be",
                      "became",
                      "because",
                      "become",
                      "becomes",
                      "becoming",
                      "been",
                      "before",
                      "beforehand",
                      "behind",
                      "being",
                      "below",
                      "beside",
                      "besides",
                      "between",
                      "beyond",
                      "bill",
                      "both",
                      "bottom",
                      "but",
                      "by",
                      "call",
                      "can",
                      "cannot",
                      "cant",
                      "co",
                      "con",
                      "could",
                      "couldnt",
                      "cry",
                      "de",
                      "describe",
                      "detail",
                      "do",
                      "done",
                      "down",
                      "due",
                      "during",
                      "eg",
                      "eight",
                      "either",
                      "eleven",
                      "else",
                      "elsewhere",
                      "empty",
                      "enough",
                      "etc",
                      "even",
                      "ever",
                      "every",
                      "everyone",
                      "everything",
                      "everywhere",
                      "except",
                      "fill",
                      "find",
                      "fire",
                      "first",
                      "five",
                      "for",
                      "former",
                      "formerly",
                      "forty",
                      "found",
                      "four",
                      "from",
                      "front",
                      "full",
                      "further",
                      "get",
                      "give",
                      "go",
                      "had",
                      "has",
                      "hasnt",
                      "have",
                      "having",
                      "hence",
                      "her",
                      "here",
                      "hereafter",
                      "hereby",
                      "herein",
                      "hereupon",
                      "hers",
                      "herself",
                      "him",
                      "himself",
                      "his",
                      "how",
                      "however",
                      "hundred",
                      "ie",
                      "if",
                      "in",
                      "inc",
                      "indeed",
                      "interest",
                      "into",
                      "is",
                      "it",
                      "its",
                      "itself",
                      "keep",
                      "last",
                      "latter",
                      "latterly",
                      "least",
                      "less",
                      "ltd",
                      "made",
                      "many",
                      "may",
                      "me",
                      "meanwhile",
                      "might",
                      "mill",
                      "mine",
                      "more",
                      "moreover",
                      "most",
                      "mostly",
                      "move",
                      "much",
                      "must",
                      "my",
                      "myself",
                      "name",
                      "namely",
                      "neither",
                      "never",
                      "nevertheless",
                      "next",
                      "nine",
                      "no",
                      "nobody",
                      "none",
                      "noone",
                      "nor",
                      "not",
                      "nothing",
                      "now",
                      "nowhere",
                      "of",
                      "off",
                      "often",
                      "on",
                      "once",
                      "one",
                      "only",
                      "onto",
                      "or",
                      "other",
                      "others",
                      "otherwise",
                      "our",
                      "ours",
                      "ourselves",
                      "out",
                      "over",
                      "own",
                      "part",
                      "per",
                      "perhaps",
                      "please",
                      "put",
                      "rather",
                      "re",
                      "same",
                      "see",
                      "seem",
                      "seemed",
                      "seeming",
                      "seems",
                      "serious",
                      "several",
                      "she",
                      "should",
                      "show",
                      "side",
                      "since",
                      "sincere",
                      "six",
                      "sixty",
                      "so",
                      "some",
                      "somehow",
                      "someone",
                      "something",
                      "sometime",
                      "sometimes",
                      "somewhere",
                      "still",
                      "such",
                      "system",
                      "take",
                      "ten",
                      "than",
                      "that",
                      "the",
                      "their",
                      "theirs",
                      "them",
                      "themselves",
                      "then",
                      "thence",
                      "there",
                      "thereafter",
                      "thereby",
                      "therefore",
                      "therein",
                      "thereupon",
                      "these",
                      "they",
                      "thick",
                      "thin",
                      "third",
                      "this",
                      "those",
                      "though",
                      "three",
                      "through",
                      "throughout",
                      "thru",
                      "thus",
                      "to",
                      "together",
                      "too",
                      "top",
                      "toward",
                      "towards",
                      "twelve",
                      "twenty",
                      "two",
                      "un",
                      "under",
                      "until",
                      "up",
                      "upon",
                      "us",
                      "very",
                      "via",
                      "was",
                      "we",
                      "well",
                      "were",
                      "what",
                      "whatever",
                      "when",
                      "whence",
                      "whenever",
                      "where",
                      "whereafter",
                      "whereas",
                      "whereby",
                      "wherein",
                      "whereupon",
                      "wherever",
                      "whether",
                      "which",
                      "while",
                      "whither",
                      "who",
                      "whoever",
                      "whole",
                      "whom",
                      "whose",
                      "why",
                      "will",
                      "with",
                      "within",
                      "without",
                      "would",
                      "yet",
                      "you",
                      "your",
                      "yours",
                      "yourself",
                      "yourselves"};
    }

    static void ensureInitialized() { std::call_once(init_flag_, initialize_internal); }

public:
    StopwordFilter() = delete;
    StopwordFilter(const StopwordFilter&) = delete;
    StopwordFilter& operator=(const StopwordFilter&) = delete;

    static bool isStopword(const std::string& term) {
        ensureInitialized();
        if (!enabled_) {
            return false;
        }
        return stopwords_.count(term);
    }

    static void setEnabled(bool enabled) {
        ensureInitialized();
        enabled_ = enabled;
    }

    static void addStopword(const std::string& word) {
        ensureInitialized();
        stopwords_.insert(word);
    }

    static void removeStopword(const std::string& word) {
        ensureInitialized();
        stopwords_.erase(word);
    }
};

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
        case FieldType::DESC:
            return "%" + token;
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