#ifndef PAGERANK_H
#define PAGERANK_H

#include <vector>
#include "core/csr.h"
#include "core/config.h"

constexpr double ErrorAllowed = 0.001;

class PageRank {
    public:
        PageRank(core::CSRMatrix &matrix_, int N);
    
        std::vector<double>& GetPageRanks() { return results_; };
    
    private:
        core::Config configuration_ = core::Config("pagerank.conf");
        core::CSRMatrix &matrix_;
        std::vector<double> results_;
};

#endif