#include "PageRank.h"

#include "core/memory.h"
#include "data/Deserialize.h"
#include "data/Document.h"
#include "data/Gzip.h"
#include "data/Reader.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <spdlog/spdlog.h>

#if __has_include(<omp.h>)
#    include <omp.h>
#endif

#define USE_DOMAIN_RANK 0

namespace {
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
}  // namespace

namespace mithril::pagerank {
core::UniquePtr<std::unordered_map<std::string, int>> LinkToNode =
    core::UniquePtr<std::unordered_map<std::string, int>>(new std::unordered_map<std::string, int>());
core::UniquePtr<std::unordered_map<int, std::string>> NodeToLink =
    core::UniquePtr<std::unordered_map<int, std::string>>(new std::unordered_map<int, std::string>());
core::UniquePtr<std::unordered_map<int, std::vector<int>>> NodeConnections =
    core::UniquePtr<std::unordered_map<int, std::vector<int>>>(new std::unordered_map<int, std::vector<int>>());
core::UniquePtr<std::unordered_map<int, data::Document>> NodeToDocument =
    core::UniquePtr<std::unordered_map<int, data::Document>>(new std::unordered_map<int, data::Document>());
core::UniquePtr<std::unordered_map<data::docid_t, int>> DocumentToNode =
    core::UniquePtr<std::unordered_map<data::docid_t, int>>(new std::unordered_map<data::docid_t, int>());
core::UniquePtr<std::vector<double>> Results = core::UniquePtr<std::vector<double>>(new std::vector<double>());
core::UniquePtr<std::vector<float>> StandardizedResults = core::UniquePtr<std::vector<float>>(new std::vector<float>());

int Nodes = 0;
size_t DocumentCount = 0;

std::string ProcessLink(const std::string& link) {
#if USE_DOMAIN_RANK == 1
    return GetLinkDomain(link);
#else
    return link;
#endif
}

int GetLinkNode(const std::string& link) {
    std::string processedLink = ProcessLink(link);
    auto it = LinkToNode->find(processedLink);
    int nodeNo;
    if (it == LinkToNode->end()) {
        nodeNo = Nodes;
        (*LinkToNode)[processedLink] = nodeNo;
        (*NodeToLink)[nodeNo] = processedLink;
        Nodes++;
    } else {
        nodeNo = it->second;
    }

    return nodeNo;
}

void Write() {
    FILE* f = fopen(OutputFile.c_str(), "wb+");
    assert(f != nullptr);

    std::vector<float>& scores = *mithril::pagerank::StandardizedResults;

    size_t written = 0;
    for (size_t i = 0; i < DocumentCount; ++i) {
        auto it = DocumentToNode->find(static_cast<data::docid_t>(i));

        uint32_t bytes;
        if (it == DocumentToNode->end()) {
            spdlog::info("Could not find result for document ID: {}. Writing a pagerank of 0.0 instead.", i);
            bytes = 0;
        } else {
            memcpy(&bytes, &scores[it->second], sizeof(bytes));
            bytes = htonl(bytes);
            written++;
        }

        fwrite(&bytes, sizeof(bytes), 1, f);
    }

    fclose(f);

    spdlog::info("Wrote results of {}/{} documents.", written, DocumentCount);
}

void PerformPageRank(core::CSRMatrix& matrix_, int N) {
    int maxIteration = Config.GetInt("max_iterations");
    double d = Config.GetDouble("decay_factor");
    double tol = 1.0 / N;

    Results->resize(N, 1.0 / N);
    StandardizedResults->resize(N);

    std::vector<double> teleport(N, (1.0 - d) / N);

    for (int iter = 0; iter < maxIteration; ++iter) {
        std::vector<double> newR = matrix_.Multiply(*Results);

        double diff = 0.0;
#pragma omp parallel for reduction(+ : diff)
        for (int i = 0; i < N; ++i) {
            newR[i] = d * newR[i] + teleport[i];
            diff += fabs(newR[i] - (*Results)[i]);
        }
        if (diff < tol) {
            break;
        }

        *Results = newR;
    }

    double min = *std::min_element(Results->begin(), Results->end());
    double max = *std::max_element(Results->begin(), Results->end());

    double range = max - min;
    for (int i = 0; i < N; ++i) {
        (*StandardizedResults)[i] = static_cast<float>(((*Results)[i] - min) / range);
    }
}

void PerformPageRank() {
    std::unordered_map<std::string, data::docid_t> linkToNode;

    spdlog::info("Starting page rank...");

    auto start = std::chrono::steady_clock::now();

    // Read PageRank info related info from all documents and setup info needed to form the CSR Matrix
    std::vector<std::string> documentPaths;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(InputDirectory)) {
        if (!entry.is_regular_file()) {
            continue;  // skip chunk dir
        }

        std::string path = entry.path().string();
        if (path == "/.DS_Store") {
            continue;
        }
        documentPaths.push_back(std::move(path));
    }
    std::sort(documentPaths.begin(), documentPaths.end());

    for (const auto& path : documentPaths) {
        size_t docID = 0;
        try {
            docID = stoi(path.substr(path.size() - 10));
        } catch (std::exception& e) {
            continue;
        }

        if (docID != DocumentCount) {
            spdlog::error("There is a hole in the documents starting at ID: {}. Ensure all data is present.", docID);
            exit(1);
        }

        DocumentCount++;
    }

    for (const auto& path : documentPaths) {
        try {
            data::Document doc;
            {
                data::FileReader file{path.c_str()};
                data::GzipReader gzip{file};
                if (!data::DeserializeValue(doc, gzip)) {
                    throw std::runtime_error("Failed to deserialize document: " + path);
                }
            }

            int fromNode = GetLinkNode(doc.url);
            auto& vec = (*NodeConnections)[fromNode];

            for (const std::string& link : doc.forwardLinks) {
                vec.push_back(GetLinkNode(link));
            }

            (*DocumentToNode)[doc.id] = fromNode;
            (*NodeToDocument)[fromNode] = std::move(doc);
        } catch (const std::exception& e) {
            spdlog::error("Error processing {}: {}", path, e.what());
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto processDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    spdlog::info(
        "Finished processing {} documents. Found {} links. Time taken: {} ms.", DocumentCount, Nodes, processDuration);

    // Build CSR Matrix from the above data.
    // This tolerance is dynamic based on number of nodes.
    const double tol = 1.0 / Nodes;
    spdlog::info("Building CSR Matrix with tolerance {:e}...", tol);

    start = std::chrono::steady_clock::now();

    core::CSRMatrix m(Nodes);
    std::vector<double> outDegree(Nodes, 0.0);

    size_t edges = 0;
    for (auto& [node, value] : *NodeConnections) {
        for (auto target : value) {
            m.AddEdge(target, node, 1.0);
            edges++;
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

    spdlog::info("Finished CSR matrix building process. Edges: {}. Time taken: {} ms", edges, csrMatrixDuration);
    spdlog::info("Performing page rank...");

    start = std::chrono::steady_clock::now();

    pagerank::PerformPageRank(m, Nodes);

    end = std::chrono::steady_clock::now();
    auto duration = (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    spdlog::info("Finished pagerank in: {} ms", duration);

    spdlog::info("Writing pagerank results to {}...", OutputFile);

    Write();

    end = std::chrono::steady_clock::now();
    auto writeDuration = (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    spdlog::info("Finished writing pagerank results to {}. Time taken: {} ms", OutputFile, writeDuration);

    spdlog::info("Total time taken: {} ms", (duration + csrMatrixDuration + processDuration + writeDuration));
}

void Cleanup() {
    LinkToNode.Reset(nullptr);
    NodeToLink.Reset(nullptr);
    NodeConnections.Reset(nullptr);
    NodeToDocument.Reset(nullptr);
    DocumentToNode.Reset(nullptr);
    Results.Reset(nullptr);
    StandardizedResults.Reset(nullptr);
}

}  // namespace mithril::pagerank