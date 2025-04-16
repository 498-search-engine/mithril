#ifndef PAGERANK_READER_H
#define PAGERANK_READER_H

#include "data/Document.h"

namespace mithril::pagerank {
class PageRankReader {
public:
    PageRankReader();
    ~PageRankReader();

    float GetDocumentPageRank(data::docid_t docid);

    uint32_t start;
    uint64_t size_;

private:
    void* map_ = nullptr;
};
}  // namespace mithril::pagerank
#endif