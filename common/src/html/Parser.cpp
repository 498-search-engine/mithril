// HtmlParser.cpp
// Authors: anubhava, madhavss

#include "html/Parser.h"

#include "core/memory.h"
#include "html/Entity.h"
#include "html/Tags.h"
#include "http/URL.h"

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

std::string_view DecodeStringWithRef(std::string_view s, std::vector<core::UniquePtr<std::string>>& decodedWords) {
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

    if (needsDecode) {
        word = DecodeStringWithRef(word, decodedWords);
    }

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

        while (start < end && !IsSpace(*start) && *start != '>') {
            if (*start == '"' && start[-1] == '=') {
                ++start;
                while (start < end && *start != '"' && start[-1] != '\\') {
                    ++start;
                }
                continue;
            }
            ++start;
        }
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
                            std::map<std::string_view, std::string_view>& metas,
                            std::string_view& base,
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
                href = DecodeStringWithRef(http::DecodeURL(href), decodedWords);
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
                base = DecodeStringWithRef(http::DecodeURL(base), decodedWords);
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
                src = DecodeStringWithRef(http::DecodeURL(src), decodedWords);
                links.emplace_back(src);
            }
            return AfterEndingOfTag(attrEnd, bufferEnd);
        }

    case DesiredAction::Meta:
        {
            if (endTag) {
                return AfterEndingOfTag(nameEnd, bufferEnd);
            }

            std::string_view name;
            const char* attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "name=", name);
            if (!attrEnd || name.empty()) {
                attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "property=", name);
                if (!attrEnd) {
                    return nullptr;
                }
            }
            std::string_view content;
            attrEnd = ProcessTagAttributes(nameStart, bufferEnd, "content=", content);
            if (!attrEnd) {
                return nullptr;
            }

            if (!name.empty() && !content.empty()) {
                metas[name] = content;
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
    auto& metas = parsed.metas;
    auto& base = parsed.base;
    auto& decodedWords = parsed.decodedWords;

    words.clear();
    titleWords.clear();
    links.clear();
    metas.clear();
    base = std::string_view{};
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
            buffer = HandleTagAction(
                action, endTag, nameStart, nameEnd, bufferEnd, state, currentLink, links, metas, base, decodedWords);
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

    if (state.inAnchor && !currentLink.url.empty()) {
        links.emplace_back(std::move(currentLink));
        currentLink.url = ""sv;
    }
}

}  // namespace mithril::html
