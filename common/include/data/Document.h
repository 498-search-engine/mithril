#ifndef COMMON_DATA_DOCUMENT_H
#define COMMON_DATA_DOCUMENT_H

#include "data/Deserialize.h"
#include "data/Reader.h"
#include "data/Serialize.h"
#include "data/Writer.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mithril::data {

using docid_t = uint32_t;

enum class FieldType {
    BODY = 0,
    TITLE = 1,
    URL = 2,
    ANCHOR = 3,
    DESC = 4
    // Can be extended with HEADING, BOLD, etc.
};

struct DocInfo {
    data::docid_t id;
    uint32_t url_offset;
    uint32_t url_length;
    uint32_t title_offset;
    uint32_t title_length;
    uint32_t body_length;
    uint32_t desc_length;
    float pagerank_score;

    uint32_t getFieldLength(FieldType field) const {
        switch (field) {
        case FieldType::BODY:
            return body_length;
        case FieldType::TITLE:
            return title_length;
        case FieldType::URL:
            return url_length;
        case FieldType::DESC:
            return desc_length;
        default:
            return 0;
        }
    }
};

struct Document {
    docid_t id;
    std::string url;
    std::vector<std::string> title;
    std::vector<std::string> description;
    std::vector<std::string> words;
    std::vector<std::string> forwardLinks;
};

struct DocumentView {
    docid_t id;
    std::string url;
    const std::vector<std::string_view>& title;
    const std::vector<std::string_view>& description;
    const std::vector<std::string_view>& words;
    const std::vector<std::string>& forwardLinks;
};

template<>
struct Serialize<Document> {
    template<Writer W>
    static void Write(const Document& doc, W& w) {
        SerializeValue(doc.id, w);
        SerializeValue(doc.url, w);
        SerializeValue(doc.title, w);
        SerializeValue(doc.description, w);
        SerializeValue(doc.words, w);
        SerializeValue(doc.forwardLinks, w);
    }
};

template<>
struct Serialize<DocumentView> {
    template<Writer W>
    static void Write(const DocumentView& doc, W& w) {
        SerializeValue(doc.id, w);
        SerializeValue(doc.url, w);
        SerializeValue(doc.title, w);
        SerializeValue(doc.description, w);
        SerializeValue(doc.words, w);
        SerializeValue(doc.forwardLinks, w);
    }
};

template<>
struct Deserialize<Document> {
    template<Reader R>
    static bool Read(Document& doc, R& r) {
        // clang-format off
        return DeserializeValue(doc.id, r)
            && DeserializeValue(doc.url, r)
            && DeserializeValue(doc.title, r)
            && DeserializeValue(doc.description, r)
            && DeserializeValue(doc.words, r)
            && DeserializeValue(doc.forwardLinks, r);
        // clang-format on
    }
};

}  // namespace mithril::data

#endif
