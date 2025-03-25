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

struct Document {
    docid_t id;
    std::string url;
    std::vector<std::string> title;
    std::vector<std::string> description;
    std::vector<std::string> words;
};

struct DocumentView {
    docid_t id;
    std::string url;
    const std::vector<std::string_view>& title;
    const std::vector<std::string_view>& description;
    const std::vector<std::string_view>& words;
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
            && DeserializeValue(doc.words, r);
        // clang-format on
    }
};

}  // namespace mithril::data

#endif
