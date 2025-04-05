#ifndef INDEX_DOCUMENTMAPREADER_H
#define INDEX_DOCUMENTMAPREADER_H

#include "TextPreprocessor.h"
#include "data/Document.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mithril {

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

class DocumentMapReader {
public:
    explicit DocumentMapReader(const std::string& index_dir);

    // core func
    std::optional<data::Document> getDocument(data::docid_t id) const;
    std::optional<data::docid_t> lookupDocID(const std::string& url) const;

    // iterator func
    bool hasNext() const;
    data::Document next();
    void reset();

    // utils func
    size_t documentCount() const { return doc_count_; }
    const std::vector<DocInfo>& getDocInfos() const { return doc_infos_; }

private:
    std::vector<DocInfo> doc_infos_;
    std::string urls_;
    std::string titles_;

    std::unordered_map<std::string_view, data::docid_t> url_to_id_;
    std::unordered_map<data::docid_t, size_t> id_to_index_;  // doc_id: index in doc_infos_

    size_t current_position_{0};
    size_t doc_count_{0};

    void loadDocumentMap(const std::string& path);
};

}  // namespace mithril

#endif  // INDEX_DOCUMENTMAPREADER_H
