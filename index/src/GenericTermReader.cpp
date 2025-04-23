#include "GenericTermReader.h"

#include "TermOR.h"
#include "IndexStreamReader.h"
#include "TermReader.h"

#include <array>
#include <string_view>
#include <vector>
#include <memory>

static constexpr std::array<std::string_view, 4> kDecorators = {"", "@", "$", "%"};

namespace mithril {

GenericTermReader::GenericTermReader(const std::string& term,
                                     const core::MemMapFile& index_file,
                                     TermDictionary& term_dict,
                                     PositionIndex& position_index)
    : term_(term), index_file_(index_file), term_dict_(term_dict), position_index_(position_index)
{
    std::vector<std::unique_ptr<IndexStreamReader>> readers;
    for (const auto& decorator: kDecorators) {
        const auto decorated_term = std::string(decorator) + term; // TODO: don't use operator+
        auto ptr = new TermReader(/*TODO: remove path*/"", decorated_term, index_file_, term_dict_, position_index_);
        readers.emplace_back(reinterpret_cast<IndexStreamReader*>(ptr));
    }

    term_reader_ = std::make_unique<TermOR>(std::move(readers));
}

bool GenericTermReader::hasNext() const {
    return term_reader_->hasNext();
}

void GenericTermReader::moveNext() {
    return term_reader_->moveNext();
}

data::docid_t GenericTermReader::currentDocID() const {
    return term_reader_->currentDocID();
}

void GenericTermReader::seekToDocID(data::docid_t target_doc_id) {
    return term_reader_->seekToDocID(target_doc_id);
}

}  // namespace mithril