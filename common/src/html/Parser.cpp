// HtmlParser.cpp
// Authors: anubhava, madhavss

#include "html/Parser.h"

#include "html/Tags.h"

#include <cassert>
#include <cstring>

namespace mithril::html {

using namespace mithril::html::internal;

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

bool CompareTagName(const char* start1, const char* end1, const char* start2, const char* end2) {
    if (!start1 || !end1 || !start2 || !end2)
        return false;
    size_t len1 = end1 - start1;
    size_t len2 = end2 - start2;
    return len1 == len2 && std::strncmp(start1, start2, len1) == 0;
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

void CollectWord(std::string& word,
                 const ParserState& state,
                 std::vector<std::string>& words,
                 std::vector<std::string>& titleWords,
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
    word.clear();
}

const char* ProcessTagAttributes(const char* start, const char* end, const char* attr, std::string& result) {
    result.clear();
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
                    result = std::string(attr_start, start);
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
                            std::string& base) {
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
                    currentLink = Link("");
                    state.inAnchor = false;
                }
                return AfterEndingOfTag(nameEnd, bufferEnd);
            }

            std::string href;
            const char* attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "href=", href);
            if (!attrEnd)
                return nullptr;

            if (!href.empty()) {
                if (state.inAnchor) {
                    links.emplace_back(std::move(currentLink));
                }
                currentLink = Link(std::move(href));
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
            std::string src;
            const char* attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "src=", src);
            if (!attrEnd)
                return nullptr;
            if (!src.empty()) {
                links.emplace_back(std::move(src));
            }
            return AfterEndingOfTag(attrEnd, bufferEnd);
        }

    default:  // OrdinaryText
        return nameEnd;
    }
}
}  // namespace

Parser::Parser(const char* buffer, size_t length) {
    words.reserve(50000);
    titleWords.reserve(10000);
    links.reserve(20000);

    ParserState state;
    const char* const bufferEnd = buffer + length;
    std::string currentWord;
    currentWord.reserve(5000);
    Link currentLink("");

    while (buffer < bufferEnd) {
        // Skip whitespace
        if (IsSpace(*buffer)) {
            CollectWord(currentWord, state, words, titleWords, currentLink);
            while (buffer < bufferEnd && IsSpace(*buffer))
                buffer++;
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
                currentWord += *buffer;
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
                continue;
            }

            DesiredAction action = LookupPossibleTag(nameStart, nameEnd);

            if (action == DesiredAction::OrdinaryText) {
                // Not a real tag - just add to current word
                currentWord += *buffer;
                buffer++;
                continue;
            }

            // Real tag - collect current word first
            CollectWord(currentWord, state, words, titleWords, currentLink);

            // Now process the tag
            buffer = HandleTagAction(action, endTag, nameStart, nameEnd, bufferEnd, state, currentLink, links, base);
            if (!buffer)
                return;
            continue;
        }

        // Normal text processing
        if (!state.discardSection) {
            currentWord += *buffer;
        }
        buffer++;
    }

    // Handle any remaining word and link
    CollectWord(currentWord, state, words, titleWords, currentLink);
    if (state.inAnchor && !currentLink.URL.empty()) {
        links.emplace_back(std::move(currentLink));
    }
}

}  // namespace mithril::html
