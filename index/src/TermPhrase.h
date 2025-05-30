#ifndef INDEX_TERMPHRASE_H
#define INDEX_TERMPHRASE_H

#include "DocumentMapReader.h"
#include "IndexStreamReader.h"
#include "PositionIndex.h"
#include "TermAND.h"
#include "TermDictionary.h"
#include "TermReader.h"
#include "core/mem_map_file.h"
#include "data/Document.h"

#include <memory>
#include <string>
#include <vector>

namespace mithril {

class TermPhrase : public IndexStreamReader {
public:
    explicit TermPhrase(const std::string& index_path,
                        const std::vector<std::string>& phrase,
                        const core::MemMapFile& index_file,
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
    const std::string& index_path_;
    const std::vector<std::string>& phrase_;
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