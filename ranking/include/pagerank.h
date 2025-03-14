#ifndef PAGERANK_H
#define PAGERANK_H

constexpr double ErrorAllowed = 0.001;

class PageRank {
    public:
        PageRank(float *matrix, int rows, int columns);
    
        void GetPageRanks();
    
    private:
        float *matrix_;
        int num_rows_;
        int num_columns_;
};

#endif