#ifndef INDEX_IDENTITY_ISR_H
#define INDEX_IDENTITY_ISR_H

#include "IndexStreamReader.h"

#include <exception>

namespace mithril {

/**
 * @brief Identity ISR
 * Used for various edge cases to do with empty posting lists.
 * NOTE: calling currentDocID() is undefined behaviour
 * 
 */
class IdentityISR final : public IndexStreamReader {
public:
    bool hasNext() const override { return false; }
    void moveNext() override {}

    // WARNING: not valid
    data::docid_t currentDocID() const override { throw std::runtime_error("Tried to read empty ISR"); }
    void seekToDocID(data::docid_t target_doc_id) override {}

    bool isIdentity() const override { return true; }
};

}  // namespace mithril

#endif
