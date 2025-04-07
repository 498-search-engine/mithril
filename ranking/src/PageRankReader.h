#ifndef PAGERANK_READER_H
#define PAGERANK_READER_H

#include "data/Document.h"

namespace mithril::pagerank {
class PageRankReader {
public:
    PageRankReader();

    double GetDocumentPageRank(data::docid_t docid);

    uint64_t size_;

private:
    double* map_;
};
}  // namespace mithril::pagerank
#endif