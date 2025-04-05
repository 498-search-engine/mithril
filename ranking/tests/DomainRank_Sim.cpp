#include "PageRank.h"
#include "data/Deserialize.h"
#include "data/Document.h"
#include "data/Gzip.h"
#include "data/Reader.h"

#include <filesystem>
#include <fstream>
#include <numeric>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace {
core::Config Config = core::Config("tests.conf");
const std::string InputDirectory = std::string(Config.GetString("simulation_input_index_data_folder").Cstr());
const std::string OutputFile = std::string(Config.GetString("domainrank_sim_out").Cstr());
std::unordered_map<std::string, int> LinkToNode;
std::unordered_map<int, std::string> NodeToLink;
std::unordered_map<int, std::vector<int>> NodeConnections;
int Nodes = 0;

using namespace mithril;

std::string GetLinkDomain(const std::string& link) {
    int c = 0;
    std::string ret;

    for (char ch : link) {
        ret += ch;

        if (ch == '/') {
            c++;
        }

        if (c == 3) {
            break;
        }
    }

    return ret;
}

int GetLinkNode(const std::string& link) {
    std::string domain = GetLinkDomain(link);
    auto it = LinkToNode.find(domain);
    int nodeNo;
    if (it == LinkToNode.end()) {
        nodeNo = Nodes;
        LinkToNode[domain] = nodeNo;
        NodeToLink[nodeNo] = domain;
        Nodes++;
    } else {
        nodeNo = it->second;
    }

    return nodeNo;
}

void Process() {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(InputDirectory)) {
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
            auto& vec = NodeConnections[fromNode];

            for (const std::string& link : doc.forwardLinks) {
                vec.push_back(GetLinkNode(link));
            }
        } catch (const std::exception& e) {
            spdlog::error("\nError processing {}: {}", entry.path().string(), e.what());
        }
    }
}
}  // namespace

int main(int /*argc*/, char* /*argv*/[]) {
#if !defined(NDEBUG)
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    auto start = std::chrono::steady_clock::now();

    spdlog::info("Starting domain rank forward links simulation...");

    Process();
    const double tol = 1.0 / Nodes;

    auto end = std::chrono::steady_clock::now();
    auto processDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    spdlog::info("Finished processing documents. Found {} links. Time taken: {} ms.", Nodes, processDuration);

    spdlog::info("Building CSR Matrix with tolerance {:e}", tol);

    start = std::chrono::steady_clock::now();

    core::CSRMatrix m(Nodes);
    std::vector<double> outDegree(Nodes, 0.0);

    for (auto& [node, value] : NodeConnections) {
        for (auto target : value) {
            m.AddEdge(target, node, 1.0);
        }

        outDegree[node] = static_cast<double>(value.size());
    }

    m.Finalize();

    for (int i = 0; i < m.values_.size(); ++i) {
        if (outDegree[m.col_idx_[i]] > 0) {
            m.values_[i] /= outDegree[m.col_idx_[i]];
        }
    }

    end = std::chrono::steady_clock::now();
    auto csrMatrixDuration = (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    spdlog::info("Finished CSR matrix building process. Time taken: {} ms", csrMatrixDuration);
    spdlog::info("Performing domain rank....");

    start = std::chrono::steady_clock::now();

    PageRank algo(m, Nodes);

    end = std::chrono::steady_clock::now();
    auto duration = (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    spdlog::info("Finished domainrank in: {} ms", duration);
    spdlog::info("Total time taken: {} ms", (duration + csrMatrixDuration + processDuration));

    std::ofstream outFile;
    outFile.open(OutputFile);

    std::vector<double> scores = algo.GetPageRanks();
    std::vector<size_t> idx(scores.size());
    std::iota(idx.begin(), idx.end(), 0);

    stable_sort(idx.begin(), idx.end(), [&scores](size_t i1, size_t i2) { return scores[i1] < scores[i2]; });

    for (size_t i = 0; i < idx.size(); ++i) {
        outFile << NodeToLink[static_cast<int>(idx[i])] << ": " << scores[idx[i]] << std::endl;
    }

    outFile.close();

    spdlog::info("Finished writing to file: {}", OutputFile);
    return 0;
}