#include "TermQuote.h"

#include "TermReader.h"
#include "TermAND.h"

namespace mithril {

TermQuote::TermQuote(DocumentMapReader& doc_reader, const std::string& index_path,
                     const std::vector<std::string>& quote)
    : doc_reader_(doc_reader), index_path_(index_path), quote_(quote)
{
    std::vector<std::unique_ptr<TermReader>> term_readers;
    for (const auto& term: quote) term_readers.emplace_back(index_path_, term);
    stream_reader_ = std::make_unique<TermAND>(std::move(term_readers));
}

bool TermQuote::hasNext() const {
    return !at_end_;
}

void TermQuote::moveNext() {
    
}

data::docid_t TermQuote::currentDocID() const {
    
}

void TermQuote::seekToDocID(data::docid_t target_doc_id) {
    
}

bool TermQuote::findNextMatch() {
    while (stream_reader_->hasNext()) {
        stream_reader_->moveNext();
        
    }
    return false;
}


}  // namespace mithril
