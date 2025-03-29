#ifndef PAGERANK_H
#define PAGERANK_H

#include "core/config.h"
#include "core/csr.h"

#include <vector>

constexpr double ErrorAllowed = 0.001;

class PageRank {
public:
    PageRank(core::CSRMatrix& matrix_, int N);

    std::vector<double>& GetPageRanks() { return results_; };

private:
    core::Config configuration_ = core::Config("pagerank.conf");
    core::CSRMatrix& matrix_;
    std::vector<double> results_;
};

#endif