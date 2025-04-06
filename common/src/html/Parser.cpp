// HtmlParser.cpp
// Authors: anubhava, madhavss

#include "html/Parser.h"

#include "Util.h"
#include "core/memory.h"
#include "html/Entity.h"
#include "html/Tags.h"
#include "http/URL.h"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mithril::html {

using namespace mithril::html::internal;
using namespace std::string_view_literals;

namespace {

constexpr size_t MaxLinksInADocument = 5000;

bool IsSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

const char* NameEndingOfTag(const char* start, const char* end) {
    while (start < end && !IsSpace(*start) && *start != '>') {
        // Detect comment start in case like <!--asdf-->
        if (end - start >= 3 && start[0] == '!' && start[1] == '-' && start[2] == '-') {
            return start + 3;
        }
        start++;
    }
    return start;
}

const char* EndingOfTag(const char* start, const char* end) {
    while (start < end && *start != '>') {
        if (*start == '"' || *start == '\'') {
            char quote = *start;
            start++;
            const auto* attrStart = start;

            // Consume string until closing quote
            while (start < end && *start != quote) {
                start++;
            }
        }
        start++;
    }
    return start < end ? start : nullptr;
}

const char* AfterEndingOfTag(const char* start, const char* end) {
    const auto* result = EndingOfTag(start, end);
    if (result != nullptr) {
        return result + 1;
    }
    return nullptr;
}

struct ParserState {
    bool inTitle = false;
    bool inAnchor = false;
    bool discardSection = false;
    bool baseDone = false;
    // discardStart/End point to the tag name that started a discard section
    // Used to match with corresponding end tag
    const char* discardStart = nullptr;
    const char* discardEnd = nullptr;
};

std::string_view DecodeStringWithRef(std::string_view s, std::vector<core::UniquePtr<std::string>>& decodedWords) {
    if (s.empty()) {
        return {};
    }
    auto decoded = DecodeHtmlString(s);
    decodedWords.push_back(core::MakeUnique<std::string>(std::move(decoded)));
    return std::string_view{*decodedWords.back()};
}

void CollectWord(std::string_view word,
                 const ParserState& state,
                 std::vector<std::string_view>& words,
                 std::vector<std::string_view>& titleWords,
                 Link& currentLink,
                 std::vector<core::UniquePtr<std::string>>& decodedWords,
                 bool needsDecode) {
    if (word.empty())
        return;

    std::vector<std::string_view> wordsInWord;
    wordsInWord.reserve(1);

    if (needsDecode) {
        word = DecodeStringWithRef(word, decodedWords);
        wordsInWord = GetWords(word);
    } else {
        wordsInWord.push_back(word);
    }

    for (auto subWord : wordsInWord) {
        if (state.inAnchor) {
            currentLink.anchorText.push_back(subWord);
        }
        if (state.inTitle) {
            titleWords.push_back(subWord);
        } else {
            words.push_back(subWord);
        }
    }
}

std::string_view ProcessTagAttributes(const char* start, const char* end, std::string_view attr) {
    while (start < end) {
        // Consume whitespace
        while (start < end && IsSpace(*start)) {
            start++;
        }

        if (start >= end || *start == '>') {
            // Reached end/closing
            return {};
        }

        auto remaining = end - start;
        if (remaining >= attr.size() + 1 && std::strncmp(start, attr.data(), attr.size()) == 0 &&
            start[attr.size()] == '=') {
            start += attr.size() + 1;

            // Consume whitespace after =
            while (start < end && IsSpace(*start)) {
                start++;
            }

            if (*start == '"' || *start == '\'') {
                char quote = *start;
                start++;
                const auto* attrStart = start;

                // Consume string until closing quote
                while (start < end && *start != quote) {
                    start++;
                }

                if (start < end) {
                    return std::string_view{attrStart, static_cast<size_t>(start - attrStart)};
                }
            }
        }

        // Skip non-matching attribute
        while (start < end && !IsSpace(*start) && *start != '>') {
            if ((*start == '"' || *start == '\'') && start[-1] == '=') {
                char quote = *start;
                ++start;
                while (start < end && *start != quote) {
                    ++start;
                }
                continue;
            }
            ++start;
        }
    }

    return {};
}

const char* HandleTagAction(DesiredAction action,
                            bool endTag,
                            const char* nameStart,
                            const char* nameEnd,
                            const char* bufferEnd,
                            ParserState& state,
                            Link& currentLink,
                            std::vector<Link>& links,
                            std::map<std::string_view, std::string_view>& metas,
                            std::string_view& base,
                            std::string_view& lang,
                            std::vector<core::UniquePtr<std::string>>& decodedWords) {
    const char* end;

    switch (action) {
    case DesiredAction::Discard:
        return AfterEndingOfTag(nameEnd, bufferEnd);

    case DesiredAction::Title:
        state.inTitle = !endTag;
        return AfterEndingOfTag(nameEnd, bufferEnd);

    case DesiredAction::Comment:
        if (endTag)
            return nameEnd;
        end = EndingOfTag(nameEnd, bufferEnd);
        while (end && (end[-2] != '-' || end[-1] != '-')) {
            end = EndingOfTag(end + 1, bufferEnd);
        }
        return end ? end + 1 : nullptr;

    case DesiredAction::DiscardSection:
        if (endTag)
            return AfterEndingOfTag(nameEnd, bufferEnd);
        state.discardStart = nameStart;
        state.discardEnd = nameEnd;
        state.discardSection = true;
        return AfterEndingOfTag(nameEnd, bufferEnd);

    case DesiredAction::Anchor:
        {
            if (endTag) {
                if (state.inAnchor && links.size() < MaxLinksInADocument) {
                    links.emplace_back(std::move(currentLink));
                    currentLink = Link{.url = ""sv, .anchorText = {}};
                    state.inAnchor = false;
                }
                return AfterEndingOfTag(nameEnd, bufferEnd);
            }

            auto href = ProcessTagAttributes(nameStart, bufferEnd, "href"sv);
            if (!href.empty()) {
                if (state.inAnchor && links.size() < MaxLinksInADocument) {
                    links.emplace_back(std::move(currentLink));
                }
                href = DecodeStringWithRef(http::DecodeURL(href), decodedWords);
                currentLink = Link{.url = href, .anchorText = {}};
                state.inAnchor = true;
            }

            return AfterEndingOfTag(nameStart, bufferEnd);
        }

    case DesiredAction::Base:
        {
            if (endTag)
                return AfterEndingOfTag(nameEnd, bufferEnd);
            if (!state.baseDone) {
                auto rawBase = ProcessTagAttributes(nameStart, bufferEnd, "href"sv);
                base = DecodeStringWithRef(http::DecodeURL(rawBase), decodedWords);
                state.baseDone = true;
                return AfterEndingOfTag(nameStart, bufferEnd);
            }
            return AfterEndingOfTag(nameEnd, bufferEnd);
        }

    case DesiredAction::Embed:
        {
            if (endTag)
                return AfterEndingOfTag(nameEnd, bufferEnd);
            std::string_view src = ProcessTagAttributes(nameStart, bufferEnd, "src"sv);
            if (!src.empty() && links.size() < MaxLinksInADocument) {
                src = DecodeStringWithRef(http::DecodeURL(src), decodedWords);
                links.emplace_back(src);
            }
            return AfterEndingOfTag(nameStart, bufferEnd);
        }

    case DesiredAction::Meta:
        {
            if (endTag) {
                return AfterEndingOfTag(nameEnd, bufferEnd);
            }

            auto name = ProcessTagAttributes(nameStart, bufferEnd, "name"sv);
            if (name.empty()) {
                // Fall back to "property"
                name = ProcessTagAttributes(nameStart, bufferEnd, "property"sv);
            }

            auto contentRaw = ProcessTagAttributes(nameStart, bufferEnd, "content"sv);
            auto content = DecodeStringWithRef(contentRaw, decodedWords);
            if (!name.empty() && !content.empty()) {
                metas[name] = content;
            }
            return AfterEndingOfTag(nameStart, bufferEnd);
        }

    case DesiredAction::HTML:
        {
            if (endTag) {
                return AfterEndingOfTag(nameEnd, bufferEnd);
            }
            lang = ProcessTagAttributes(nameStart, bufferEnd, "lang"sv);
            return AfterEndingOfTag(nameStart, bufferEnd);
        }

    default:  // OrdinaryText
        return nameEnd;
    }
}
}  // namespace

void ParseDocument(std::string_view doc, ParsedDocument& parsed) {
    const char* buffer = doc.data();
    const auto length = doc.length();

    auto& words = parsed.words;
    auto& titleWords = parsed.titleWords;
    auto& links = parsed.links;
    auto& metas = parsed.metas;
    auto& base = parsed.base;
    auto& lang = parsed.lang;
    auto& decodedWords = parsed.decodedWords;

    words.clear();
    titleWords.clear();
    links.clear();
    metas.clear();
    base = std::string_view{};
    lang = std::string_view{};
    decodedWords.clear();

    ParserState state;
    const char* const bufferEnd = buffer + length;

    size_t currentWordStart = 0;
    size_t currentWordLength = 0;

    auto currentLink = Link{};
    bool needsDecode = false;

    auto collectCurrentWord = [&] {
        CollectWord(doc.substr(currentWordStart, currentWordLength),
                    state,
                    words,
                    titleWords,
                    currentLink,
                    decodedWords,
                    needsDecode);
    };

    while (buffer < bufferEnd) {
        // Skip whitespace
        if (IsSpace(*buffer)) {
            collectCurrentWord();
            while (buffer < bufferEnd && IsSpace(*buffer))
                buffer++;
            currentWordStart = buffer - doc.data();
            currentWordLength = 0;
            needsDecode = false;
            continue;
        }

        if (*buffer == '<') {
            const char* nameStart = buffer + 1;
            bool endTag = false;

            if (*nameStart == '/') {
                nameStart++;
                endTag = true;
            }

            const char* nameEnd = NameEndingOfTag(nameStart, bufferEnd);
            if (!nameEnd || nameEnd >= bufferEnd) {
                // Not a valid tag end - treat as normal text
                currentWordLength++;
                buffer++;
                continue;
            }

            if (nameEnd[-1] == '/') {
                endTag = true;
                nameEnd--;
            }

            // Handle discard sections first
            if (state.discardSection) {
                if (!endTag) {
                    buffer++;
                    continue;
                }
                // Compare character by character
                const char* discardStartCpy = state.discardStart;
                const char* nameStartCpy = nameStart;
                while (nameStartCpy < nameEnd && discardStartCpy < state.discardEnd &&
                       *discardStartCpy == *nameStartCpy) {
                    discardStartCpy++;
                    nameStartCpy++;
                }
                if (discardStartCpy == state.discardEnd && nameStartCpy == nameEnd) {
                    state.discardSection = false;
                    const auto* end = EndingOfTag(nameEnd, bufferEnd);
                    if (end) {
                        buffer = end;
                    }
                    // TODO: unsure if this is correct, but probably prevents us
                    // getting stuck when EndingOfTag returns nullptr?
                    buffer++;
                } else {
                    buffer++;
                    while (buffer < bufferEnd && *buffer != '<')
                        buffer++;
                }
                currentWordStart = buffer - doc.data();
                currentWordLength = 0;
                continue;
            }

            DesiredAction action = LookupPossibleTag(nameStart, nameEnd);

            if (action == DesiredAction::OrdinaryText) {
                // Not a real tag - just add to current word
                currentWordLength++;
                buffer++;
                continue;
            }

            // Real tag - collect current word first
            collectCurrentWord();

            // Now process the tag
            buffer = HandleTagAction(action,
                                     endTag,
                                     nameStart,
                                     nameEnd,
                                     bufferEnd,
                                     state,
                                     currentLink,
                                     links,
                                     metas,
                                     base,
                                     lang,
                                     decodedWords);
            if (!buffer)
                return;

            currentWordStart = buffer - doc.data();
            currentWordLength = 0;
            needsDecode = false;
            continue;
        }

        // Normal text processing
        if (!state.discardSection) {
            if (*buffer == '&') {
                needsDecode = true;
            }
            currentWordLength++;
        }
        buffer++;
    }

    // Handle any remaining word and link
    collectCurrentWord();
    currentWordStart += currentWordLength;
    currentWordLength = 0;
    needsDecode = false;

    if (state.inAnchor && !currentLink.url.empty() && links.size() < MaxLinksInADocument) {
        links.emplace_back(std::move(currentLink));
        currentLink.url = ""sv;
    }
}

}  // namespace mithril::html
