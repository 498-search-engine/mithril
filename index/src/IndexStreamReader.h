#ifndef INDEX_INDEXSTREAMREADER_H
#define INDEX_INDEXSTREAMREADER_H

#include "data/Document.h"

#include <optional>

namespace mithril {

class IndexStreamReader {
public:
    virtual ~IndexStreamReader() = default;

    virtual bool hasNext() const = 0;
    virtual void moveNext() = 0;

    // TODO: add hasCurrent() to avoid problems with empty ISRs

    virtual data::docid_t currentDocID() const = 0;
    virtual void seekToDocID(data::docid_t target_doc_id) = 0;

    virtual bool isIdentity() const { return false; }
};

}  // namespace mithril

#endif  // INDEX_INDEXSTREAMREADER_H