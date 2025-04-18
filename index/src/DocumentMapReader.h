#ifndef INDEX_DOCUMENTMAPREADER_H
#define INDEX_DOCUMENTMAPREADER_H

#include "TextPreprocessor.h"
#include "data/Document.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace mithril {

using DocInfo = data::DocInfo;

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
    const DocInfo& getDocInfo(data::docid_t id) const { return doc_infos_[id_to_index_.at(id)]; }

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
