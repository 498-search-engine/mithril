#ifndef INDEX_GENERIC_TERMREADER
#define INDEX_GENERIC_TERMREADER

#include "IndexStreamReader.h"
#include "PositionIndex.h"
#include "TermDictionary.h"
#include "TermOR.h"
#include "core/mem_map_file.h"

#include <memory>
#include <string>

namespace mithril {

class GenericTermReader : public IndexStreamReader {
public:
    GenericTermReader(const std::string& term,
                      const core::MemMapFile& index_file,
                      TermDictionary& term_dict,
                      PositionIndex& position_index);

    ~GenericTermReader() override = default;

    bool hasNext() const override;
    void moveNext() override;

    data::docid_t currentDocID() const override;
    void seekToDocID(data::docid_t target_doc_id) override;

    // Need to do these to make phrases work
    // TODO: hasPositions() const;
    // TODO: currentPositions() const;
    // TODO: currentFrequency() const;

private:
    std::string term_;
    const core::MemMapFile& index_file_;
    TermDictionary& term_dict_;
    PositionIndex& position_index_;
    std::unique_ptr<TermOR> term_reader_;
    // TODO: make this custom instead of relying on TermOR
};

}  // namespace mithril


#endif