#ifndef INDEX_TERMQUOTE_H
#define INDEX_TERMQUOTE_H

#include "DocumentMapReader.h"
#include "IndexStreamReader.h"
#include "TermAND.h"
#include "TermDictionary.h"
#include "TermReader.h"
#include "data/Document.h"
#include "PositionIndex.h"
#include "core/mem_map_file.h"

#include <memory>
#include <string>
#include <vector>

namespace mithril {

class TermQuote : public IndexStreamReader {
public:
    explicit TermQuote(const std::string& index_path,
                       const std::vector<std::string>& quote,
                       const core::MemMapFile& index_file,
                       TermDictionary& term_dict,
                       PositionIndex& postion_index);

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
    const std::string& index_path_;
    const std::vector<std::string>& quote_;
    const core::MemMapFile& index_file_;
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