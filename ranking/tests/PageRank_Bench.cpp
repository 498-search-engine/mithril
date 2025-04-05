#include "PageRank.h"

#include <chrono>
#include <unordered_set>
#include <vector>
#include <spdlog/spdlog.h>

using namespace std;

int main(int argc, char* argv[]) {
    int nodes = 1000;  // 100 mill
    if (argc > 1) {
        nodes = stoi(argv[1]);
    }
    srand(498);

    const double tol = 1.0 / nodes;

    std::cout << "simulating " << nodes << " nodes with precision of " << tol << std::endl;

    auto start = std::chrono::steady_clock::now();

    core::CSRMatrix m(nodes);
    vector<double> outDegree(nodes, 0.0);

    // Build CSR matrix
    for (int i = 0; i < nodes; ++i) {
        int outgoingNodes = rand() % 10;
        if (outgoingNodes > 9) {
            // random spammy pages with loads of links
            outgoingNodes += 1000;
        }

        // assume pages have at least 3 links to somewhere else
        outgoingNodes += 3;
        outgoingNodes = min(outgoingNodes, nodes - 2);

        unordered_set<int> alreadyAdded;
        for (int j = 0; j < outgoingNodes; ++j) {
            int outnode = rand() % nodes;
            while (outnode == i || alreadyAdded.contains(outnode)) {
                outnode = rand() % nodes;
            }

            alreadyAdded.insert(outnode);
            m.AddEdge(outnode, i, 1.0);
        }

        outDegree[i] = outgoingNodes;
    }

    m.Finalize();

    for (int i = 0; i < m.values_.size(); ++i) {
        if (outDegree[m.col_idx_[i]] > 0) {
            m.values_[i] /= outDegree[m.col_idx_[i]];
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "finished building graph in: " << duration.count() << " ms" << std::endl;

    start = std::chrono::steady_clock::now();

    PageRank algo(m, nodes);

    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "finished pagerank in: " << duration.count() << " ms" << std::endl;

    // cout << "PageRank scores:\n";
    // for (double score : algo.GetPageRanks()) {
    //     cout << score << " ";
    // }
    // cout << endl;
    return 0;
}
