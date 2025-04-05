#ifndef INDEX_TERMQUOTE_H
#define INDEX_TERMQUOTE_H

#include "IndexStreamReader.h"
#include "DocumentMapReader.h"
#include "data/Document.h"

#include <memory>
#include <string>
#include <vector>

namespace mithril {

class TermQuote : public IndexStreamReader {
public:
    explicit TermQuote(DocumentMapReader& doc_reader, const std::string& index_path,
                       const std::vector<std::string>& quote);

    TermQuote(const TermQuote&) = delete;
    TermQuote& operator=(const TermQuote&) = delete;

    ~TermQuote() override = default;

    bool hasNext() const override;
    void moveNext() override;

    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

private:
    bool findNextMatch();

private:
    DocumentMapReader& doc_reader_;
    const std::string& index_path_;
    const std::vector<std::string>& quote_;
    std::unique_ptr<IndexStreamReader> stream_reader_;
    data::docid_t current_doc_id_{0};
    bool at_end_{false};
};

}  // namespace mithril

#endif