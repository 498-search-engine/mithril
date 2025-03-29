#include "pagerank.h"

#include "data/Deserialize.h"
#include "data/Gzip.h"
#include "data/Reader.h"
#include "data/Document.h"

#include <numeric>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <fstream>

static const std::string input_dir = "pages";
static const std::string output_file = "pageranks_out.txt";
static inline std::unordered_map<std::string, int> linkToNode;
static inline std::unordered_map<int, std::string> nodeToLink;
static inline std::unordered_map<int, std::vector<int>> nodeConnections;
static inline int nodes = 0;

using namespace mithril;

int GetLinkNode(const std::string& link) {
    auto it = linkToNode.find(link);
    int nodeNo;
    if (it == linkToNode.end()) {
        nodeNo = nodes;
        linkToNode[link] = nodeNo;
        nodeToLink[nodeNo] = link;
        nodes++;
    } else {
        nodeNo = it->second; 
    }
    
    return nodeNo;
}

void Process() {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) {
            continue;  // skip chunk dir
        }

        try {
            data::Document doc;
            {
                data::FileReader file{entry.path().string().c_str()};
                data::GzipReader gzip{file};
                if (!data::DeserializeValue(doc, gzip)) {
                    throw std::runtime_error("Failed to deserialize document: " + entry.path().string());
                }
            }

            int fromNode = GetLinkNode(doc.url);
            auto &vec = nodeConnections[fromNode];

            for (const std::string& link : doc.forwardLinks) {
                vec.push_back(GetLinkNode(link));
            }
        } catch (const std::exception& e) {
            spdlog::error("\nError processing {}: {}", entry.path().string(), e.what());
        }
    }
}

int main(int argc, char *argv[]) {    
 #if !defined(NDEBUG)
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    
    auto start = std::chrono::steady_clock::now();

    spdlog::info("Starting page rank forward links test...");

    Process();
    const double tol = 1.0 / nodes;

    auto end = std::chrono::steady_clock::now();
    auto duration = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    spdlog::info("Finished processing documents. Time taken: " + duration + "ms Links found: " + std::to_string(nodes));

    spdlog::info("Building CSR Matrix with tolerance " + std::to_string(tol));

    start = std::chrono::steady_clock::now();

    core::CSRMatrix m(nodes);
    std::vector<double> outDegree(nodes, 0.0);

    for (auto &[node, value] : nodeConnections) {
        for (auto target : value) {
            m.AddEdge(target, node, 1.0);
        }

        outDegree[node] = value.size();
    }

    m.Finalize();

    for (int i = 0; i < m.values_.size(); ++i) {
        if (outDegree[m.col_idx_[i]] > 0) {
            m.values_[i] /= outDegree[m.col_idx_[i]];
        }
    }


    end = std::chrono::steady_clock::now();
    duration = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    spdlog::info("Finished CSR matrix building process. Time taken: " + duration + "ms");
    spdlog::info("Performing page rank....");

    start = std::chrono::steady_clock::now();

    PageRank algo(m, nodes);

    end = std::chrono::steady_clock::now();
    duration = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    spdlog::info("Finished pagerank in: " + duration + "ms");

    std::ofstream out_file;
    out_file.open(output_file);

    std::vector<double> scores = algo.GetPageRanks();
    std::vector<size_t> idx(scores.size());
    std::iota(idx.begin(), idx.end(), 0);

    stable_sort(idx.begin(), idx.end(),
       [&scores](size_t i1, size_t i2) {return scores[i1] < scores[i2];});

    for (int i = 0; i < idx.size(); ++i) {
        out_file << nodeToLink[idx[i]] << ": " << scores[idx[i]] << std::endl;
    }

    out_file.close();
    // cout << "PageRank scores:\n";
    // for (double score : algo.GetPageRanks()) { 
    //     cout << score << " ";
    // }
    // cout << endl;
    return 0;
}

