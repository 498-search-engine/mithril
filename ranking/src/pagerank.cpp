#include "pagerank.h"

#include <cmath>
#include <omp.h>

PageRank::PageRank(core::CSRMatrix& matrix_, int N) : matrix_(matrix_) {
    int maxIteration = configuration_.GetInt("max_iterations");
    double d = configuration_.GetDouble("decay_factor");
    double tol = 1.0 / N;

    results_.resize(N, 1.0 / N);

    std::vector<double> teleport(N, (1.0 - d) / N);

    for (int iter = 0; iter < maxIteration; ++iter) {
        std::vector<double> newR = matrix_.Multiply(results_);

        double diff = 0.0;
#pragma omp parallel for reduction(+ : diff)
        for (int i = 0; i < N; ++i) {
            newR[i] = d * newR[i] + teleport[i];
            diff += fabs(newR[i] - results_[i]);
        }
        if (diff < tol) {
            break;
        }

        results_ = newR;
    }
}
