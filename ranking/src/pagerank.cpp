#include <iostream>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <chrono>

// for building this quickly to test, use: g++ pagerank.cpp -O3 -fopenmp -std=c++20 -o pagerank

using namespace std;

struct CSRMatrix {
    vector<int> row_ptr, col_idx;
    vector<double> values;
    int N;

    CSRMatrix(int nodes) : N(nodes) { row_ptr.resize(N + 1, 0); }

    void AddEdge(int from, int to, double weight) {
        col_idx.push_back(to);
        values.push_back(weight);
        row_ptr[from + 1]++;
    }

    void Finalize() {
        for (int i = 1; i <= N; ++i) {
            row_ptr[i] += row_ptr[i - 1];
        }
    }

    vector<double> Multiply(const vector<double>& vec) {
        vector<double> result(N, 0.0);
        #pragma omp parallel for
        for (int i = 0; i < N; ++i) {
            for (int j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
                result[i] += values[j] * vec[col_idx[j]];
            }
        }
        return result;
    }
};

vector<double> PageRank(CSRMatrix &m, int N, double d = 0.85, int max_iter = 100, double tol = 1e-6) {
    vector<double> r(N, 1.0 / N);
    vector<double> teleport(N, (1.0 - d) / N);

    for (int iter = 0; iter < max_iter; ++iter) {
        vector<double> newR = m.Multiply(r);

        double diff = 0.0;
        #pragma omp parallel for reduction(+:diff)
        for (int i = 0; i < N; ++i) {
            newR[i] = d * newR[i] + teleport[i];
            diff += fabs(newR[i] - r[i]);
        }
        if (diff < tol) { 
            break;
        }
        r = newR;
    }
    return r;
}

int main(int argc, char *argv[]) {    
    int nodes = 1000; // 100 mill
    if (argc > 1) {
        nodes = stoi(argv[1]);
    }
    srand(498);

    std::cout << "simulating " << nodes << " nodes" << std::endl;

    auto start = std::chrono::steady_clock::now();

    CSRMatrix m(nodes);
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

    for (int i = 0; i < m.values.size(); ++i) {
        if (outDegree[m.col_idx[i]] > 0) {
            m.values[i] /= outDegree[m.col_idx[i]];
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "finished building graph in: " << duration.count() << " ms" << std::endl;

    start = std::chrono::steady_clock::now();
    vector<double> result = PageRank(m, nodes);
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "finished pagerank in: " << duration.count() << " ms" << std::endl;

    // cout << "PageRank scores:\n";
    // for (double score : result) { 
    //     cout << score << " ";
    // }
    // cout << endl;
    return 0;
}
