// HtmlParser.cpp
// Authors: anubhava, madhavss

#include "html/Parser.h"

#include "html/Tags.h"

#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

namespace mithril::html {

using namespace mithril::html::internal;
using namespace std::string_view_literals;

namespace {

bool IsSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

const char* NameEndingOfTag(const char* start, const char* end) {
    while (start < end && !IsSpace(*start) && *start != '>') {
        start++;
    }
    return start;
}

const char* EndingOfTag(const char* start, const char* end) {
    while (start < end && *start != '>') {
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

void CollectWord(std::string_view word,
                 const ParserState& state,
                 std::vector<std::string_view>& words,
                 std::vector<std::string_view>& titleWords,
                 Link& currentLink) {
    if (word.empty())
        return;

    if (state.inAnchor) {
        currentLink.anchorText.push_back(word);
    }
    if (state.inTitle) {
        titleWords.push_back(word);
    } else {
        words.push_back(word);
    }
}

const char* ProcessTagAttributes(const char* start, const char* end, const char* attr, std::string_view& result) {
    const char* attr_start = nullptr;

    while (start < end) {
        while (start < end && IsSpace(*start))
            start++;
        if (start >= end || *start == '>')
            return start;

        if (std::strncmp(start, attr, std::strlen(attr)) == 0) {
            start += std::strlen(attr);
            while (start < end && IsSpace(*start))
                start++;
            if (*start == '"') {
                start++;
                attr_start = start;
                while (start < end && *start != '"')
                    start++;
                if (start < end) {
                    result = std::string_view{attr_start, static_cast<size_t>(start - attr_start)};
                    return start;
                }
            }
        }

        while (start < end && !IsSpace(*start) && *start != '>')
            start++;
    }
    return start;
}

const char* HandleTagAction(DesiredAction action,
                            bool endTag,
                            const char* nameStart,
                            const char* nameEnd,
                            const char* bufferEnd,
                            ParserState& state,
                            Link& currentLink,
                            std::vector<Link>& links,
                            std::string_view& base) {
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
                if (state.inAnchor) {
                    links.emplace_back(std::move(currentLink));
                    currentLink = Link{.url = ""sv, .anchorText = {}};
                    state.inAnchor = false;
                }
                return AfterEndingOfTag(nameEnd, bufferEnd);
            }

            std::string_view href;
            const char* attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "href=", href);
            if (!attrEnd)
                return nullptr;

            if (!href.empty()) {
                if (state.inAnchor) {
                    links.emplace_back(std::move(currentLink));
                }
                currentLink = Link{.url = href, .anchorText = {}};
                state.inAnchor = true;
            }
            return AfterEndingOfTag(attrEnd, bufferEnd);
        }

    case DesiredAction::Base:
        {
            if (endTag)
                return AfterEndingOfTag(nameEnd, bufferEnd);
            if (!state.baseDone) {
                const char* attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "href=", base);
                if (!attrEnd)
                    return nullptr;
                state.baseDone = true;
                return AfterEndingOfTag(attrEnd, bufferEnd);
            }
            return AfterEndingOfTag(nameEnd, bufferEnd);
        }

    case DesiredAction::Embed:
        {
            if (endTag)
                return AfterEndingOfTag(nameEnd, bufferEnd);
            std::string_view src;
            const char* attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "src=", src);
            if (!attrEnd)
                return nullptr;
            if (!src.empty()) {
                links.emplace_back(src);
            }
            return AfterEndingOfTag(attrEnd, bufferEnd);
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
    auto& base = parsed.base;
    words.reserve(50000);
    titleWords.reserve(10000);
    links.reserve(20000);

    ParserState state;
    const char* const bufferEnd = buffer + length;


    size_t currentWordStart = 0;
    size_t currentWordLength = 0;

    auto currentLink = Link{};

    while (buffer < bufferEnd) {
        // Skip whitespace
        if (IsSpace(*buffer)) {
            CollectWord(doc.substr(currentWordStart, currentWordLength), state, words, titleWords, currentLink);
            while (buffer < bufferEnd && IsSpace(*buffer))
                buffer++;
            currentWordStart = buffer - doc.data();
            currentWordLength = 0;
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
            CollectWord(doc.substr(currentWordStart, currentWordLength), state, words, titleWords, currentLink);

            // Now process the tag
            buffer = HandleTagAction(action, endTag, nameStart, nameEnd, bufferEnd, state, currentLink, links, base);
            if (!buffer)
                return;

            currentWordStart = buffer - doc.data();
            currentWordLength = 0;
            continue;
        }

        // Normal text processing
        if (!state.discardSection) {
            currentWordLength++;
        }
        buffer++;
    }

    // Handle any remaining word and link
    CollectWord(doc.substr(currentWordStart, currentWordLength), state, words, titleWords, currentLink);
    currentWordStart += currentWordLength;
    currentWordLength = 0;

    if (state.inAnchor && !currentLink.url.empty()) {
        links.emplace_back(std::move(currentLink));
        currentLink.url = ""sv;
    }
}

}  // namespace mithril::html
