#ifndef CRAWLER_STATE_H
#define CRAWLER_STATE_H

#include "ThreadSync.h"
#include "data/Deserialize.h"
#include "data/Document.h"
#include "data/Reader.h"
#include "data/Serialize.h"
#include "data/Writer.h"

#include <atomic>
#include <string>
#include <vector>

namespace mithril {

struct LiveState {
    std::atomic<data::docid_t> nextDocumentID;
    ThreadSync threadSync;
};

struct PersistentState {
    data::docid_t nextDocumentID;
    std::vector<std::string> pendingURLs;
};

namespace data {

template<>
struct Serialize<PersistentState> {
    template<Writer W>
    static void Write(const PersistentState& state, W& w) {
        SerializeValue(state.nextDocumentID, w);
        SerializeValue(state.pendingURLs, w);
    }
};

template<>
struct Deserialize<PersistentState> {
    template<Reader R>
    static bool Read(PersistentState& state, R& r) {
        // clang-format off
        return DeserializeValue(state.nextDocumentID, r)
            && DeserializeValue(state.pendingURLs, r);
        // clang-format on
    }
};

};  // namespace data

}  // namespace mithril

#endif
