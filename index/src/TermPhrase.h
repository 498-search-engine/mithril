#ifndef INDEX_TERMQUOTE_H
#define INDEX_TERMQUOTE_H

#include "DocumentMapReader.h"
#include "IndexStreamReader.h"
#include "TermAND.h"
#include "TermDictionary.h"
#include "TermReader.h"
#include "data/Document.h"
#include "PositionIndex.h"

#include <memory>
#include <string>
#include <vector>

namespace mithril {

class TermPhrase : public IndexStreamReader {
public:
    explicit TermPhrase(DocumentMapReader& doc_reader,
                        const std::string& index_path,
                        const std::vector<std::string>& phrase,
                        TermDictionary& term_dict,
                        PositionIndex& position_index);

    TermPhrase(const TermPhrase&) = delete;
    TermPhrase& operator=(const TermPhrase&) = delete;

    ~TermPhrase() override = default;

    bool hasNext() const override;
    void moveNext() override;

    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

private:
    bool findNextMatch();

private:
    DocumentMapReader& doc_reader_;
    const std::string& index_path_;
    const std::vector<std::string>& phrase_;
    TermDictionary& term_dict_;
    PositionIndex& position_index_;
    std::vector<TermReader*> term_readers_;  // sketchy
    std::unique_ptr<TermAND> stream_reader_;
    data::docid_t current_doc_id_{0};
    data::docid_t next_doc_id_{0};
    bool at_end_{false};
};

}  // namespace mithril

#endif