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
core::UniquePtr<std::vector<std::vector<int>>> NodeConnections =
    core::UniquePtr<std::vector<std::vector<int>>>(new std::vector<std::vector<int>>);
core::UniquePtr<std::vector<PagerankDocument>> NodeToDocument =
    core::UniquePtr<std::vector<PagerankDocument>>(new std::vector<PagerankDocument>());
core::UniquePtr<std::unordered_map<data::docid_t, int>> DocumentToNode =
    core::UniquePtr<std::unordered_map<data::docid_t, int>>(new std::unordered_map<data::docid_t, int>());
core::UniquePtr<std::vector<float>> Results = core::UniquePtr<std::vector<float>>(new std::vector<float>());
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
#if USE_DOMAIN_RANK == 1
    std::string processedLink = ProcessLink(link);
#else
    const std::string& processedLink = link;
#endif
    auto it = LinkToNode->find(processedLink);
    int nodeNo;
    if (it == LinkToNode->end()) {
        nodeNo = Nodes;
        (*LinkToNode)[processedLink] = nodeNo;
        NodeConnections->push_back(std::vector<int>());
        NodeToDocument->push_back(PagerankDocument{});
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
            spdlog::warn("Could not find result for document ID: {}. Writing a pagerank of 0.0 instead.", i);
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
    float d = Config.GetFloat("decay_factor");
    float tol = 1.0F / static_cast<float>(N);

    Results->resize(N, tol);
    StandardizedResults->resize(N);

    std::vector<float> teleport(N, (1.0F - d) / static_cast<float>(N));

    for (int iter = 0; iter < maxIteration; ++iter) {
        std::vector<float> newR = matrix_.Multiply(*Results);

        float diff = 0.0;
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

    constexpr float Epsilon = 1e-30;  // Avoid log(0)

    std::vector<float> temp;
    temp.reserve(N);
    StandardizedResults->resize(N);

    for (int i = 0; i < N; ++i) {
        temp.push_back(std::log10((*Results)[i] + Epsilon));
    }

    auto [minit, maxit] = std::minmax_element(temp.begin(), temp.end());

    float min = *minit;
    float max = *maxit;
    float range = max - min;

    // Square root twice to spread lower values more
    constexpr float Power = 0.5 * 0.5;
    for (int i = 0; i < N; ++i) {
        (*StandardizedResults)[i] = std::pow(((temp[i] - min) / range), Power);
    }
}

void PerformPageRank(const std::string& inputDirectory) {
    spdlog::info("Starting page rank...");

    auto start = std::chrono::steady_clock::now();

    {
        // Read PageRank info related info from all documents and setup info needed to form the CSR Matrix

        // This scope allows us to nicely clear document path.
        std::vector<std::string> documentPaths;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(inputDirectory)) {
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

        size_t startDocument = 0;
        for (const auto& path : documentPaths) {
            size_t docID = 0;
            try {
                docID = stoi(path.substr(path.size() - 10));
            } catch (std::exception& e) {
                continue;
            }

            if (startDocument != docID) {
                if (startDocument == 0) {
                    startDocument = docID;
                    spdlog::warn(
                        "Starting document ID: {}. Document ID: {}. Ensure all data is present.", startDocument, docID);
                } else {
                    spdlog::error("There is a hole in the documents starting at ID: {}. Ensure all data is present.",
                                  docID);
                    exit(1);
                }
            }

            DocumentCount++;
            startDocument++;
        }

        DocumentToNode->reserve(DocumentCount);

        size_t processed = 0;
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

                for (const std::string& link : doc.forwardLinks) {
                    int newNode = GetLinkNode(link);
                    (*NodeConnections)[fromNode].push_back(newNode);
                }
                (*NodeConnections)[fromNode].shrink_to_fit();

                (*DocumentToNode)[doc.id] = fromNode;
                (*NodeToDocument)[fromNode] = PagerankDocument{
                    .id = doc.id,
                    .url = std::move(doc.url),
                };
                processed++;
            } catch (const std::exception& e) {
                spdlog::error("Error processing {}: {}", path, e.what());
            }

            if ((processed % 10000 == 0 || processed == 1)) {
                auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double> processDoubleDuration = end - start;
                auto processDuration = processDoubleDuration.count();

                spdlog::info("Processed {}/{} documents so far. Found {} links. Time taken: {}s.",
                             processed,
                             DocumentCount,
                             Nodes,
                             processDuration);
            }
        }

        NodeConnections->shrink_to_fit();
        NodeToDocument->shrink_to_fit();
        LinkToNode.Reset(nullptr);
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> processDoubleDuration = end - start;
    auto processDuration = processDoubleDuration.count();

    spdlog::info(
        "Finished processing {} documents. Found {} links. Time taken: {}s.", DocumentCount, Nodes, processDuration);

    // Build CSR Matrix from the above data.
    // This tolerance is dynamic based on number of nodes.
    const float tol = 1.0F / static_cast<float>(Nodes);
    spdlog::info("Building CSR Matrix with tolerance {:e}...", tol);

    start = std::chrono::steady_clock::now();

    core::CSRMatrix m(Nodes);
    std::vector<float> outDegree(Nodes, 0.0);

    size_t edges = 0;
    for (int node = 0; node < Nodes; ++node) {
        const auto& value = (*NodeConnections)[node];
        for (auto target : value) {
            m.AddEdge(target, node, 1.0);
            edges++;
        }

        outDegree[node] = static_cast<float>(value.size());
    }

    NodeConnections.Reset(nullptr);

    m.Finalize();

    for (int i = 0; i < m.values_.size(); ++i) {
        if (outDegree[m.col_idx_[i]] > 0) {
            m.values_[i] /= outDegree[m.col_idx_[i]];
        }
    }

    end = std::chrono::steady_clock::now();
    std::chrono::duration<double> csrMatrixDoubleDuration = end - start;
    auto csrMatrixDuration = csrMatrixDoubleDuration.count();

    spdlog::info("Finished CSR matrix building process. Edges: {}. Time taken: {}s", edges, csrMatrixDuration);
    spdlog::info("Performing page rank...");

    start = std::chrono::steady_clock::now();

    pagerank::PerformPageRank(m, Nodes);

    end = std::chrono::steady_clock::now();
    std::chrono::duration<double> doubleDuration = end - start;
    auto duration = doubleDuration.count();
    spdlog::info("Finished pagerank in: {}s", duration);

    spdlog::info("Writing pagerank results to {}...", OutputFile);

    Write();

    end = std::chrono::steady_clock::now();
    std::chrono::duration<double> writeDurationDouble = end - start;
    auto writeDuration = writeDurationDouble.count();

    spdlog::info("Finished writing pagerank results to {}. Time taken: {}s", OutputFile, writeDuration);

    spdlog::info("Total time taken: {}s", (duration + csrMatrixDuration + processDuration + writeDuration));
}

void Cleanup() {
    NodeToDocument.Reset(nullptr);
    DocumentToNode.Reset(nullptr);
    Results.Reset(nullptr);
    StandardizedResults.Reset(nullptr);
}

}  // namespace mithril::pagerank