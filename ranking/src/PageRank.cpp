#include "PageRank.h"

#include "data/Deserialize.h"
#include "data/Document.h"
#include "data/Gzip.h"
#include "data/Reader.h"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <spdlog/spdlog.h>

#if __has_include(<omp.h>)
#    include <omp.h>
#endif

std::vector<double> mithril::pagerank::Results;
std::unordered_map<mithril::data::docid_t, mithril::data::Document> mithril::pagerank::NodeToDocument;

void mithril::pagerank::PerformPageRank(core::CSRMatrix& matrix_, int N) {
    int maxIteration = Config.GetInt("max_iterations");
    double d = Config.GetDouble("decay_factor");
    double tol = 1.0 / N;

    mithril::pagerank::Results.resize(N, 1.0 / N);

    std::vector<double> teleport(N, (1.0 - d) / N);

    for (int iter = 0; iter < maxIteration; ++iter) {
        std::vector<double> newR = matrix_.Multiply(mithril::pagerank::Results);

        double diff = 0.0;
#pragma omp parallel for reduction(+ : diff)
        for (int i = 0; i < N; ++i) {
            newR[i] = d * newR[i] + teleport[i];
            diff += fabs(newR[i] - mithril::pagerank::Results[i]);
        }
        if (diff < tol) {
            break;
        }

        mithril::pagerank::Results = newR;
    }
}

void mithril::pagerank::PerformPageRank() {
    std::unordered_map<std::string, data::docid_t> linkToNode;

    spdlog::info("Starting page rank...");

    auto start = std::chrono::steady_clock::now();
    data::docid_t maxNodeSeen = 0;

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

            NodeToDocument[doc.id] = doc;
            linkToNode[doc.url] = doc.id;
            maxNodeSeen = std::max(maxNodeSeen, doc.id);
            Nodes++;
        } catch (const std::exception& e) {
            spdlog::error("\nError processing {}: {}", entry.path().string(), e.what());
        }
    }

    // Create fake info for links that have not been scraped/do not exist.
    for (auto& [id, document] : NodeToDocument) {
        for (const std::string& link : document.forwardLinks) {
            if (linkToNode.find(link) == linkToNode.end()) {
                maxNodeSeen++;
                Nodes++;

                linkToNode[link] = maxNodeSeen;
                NodeToDocument[maxNodeSeen] = data::Document{
                    .id = maxNodeSeen,
                    .url = link,
                };
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto processDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    spdlog::info("Finished processing documents. Found {} links. Time taken: {} ms.", Nodes, processDuration);

    const double tol = 1.0 / Nodes;
    spdlog::info("Building CSR Matrix with tolerance {:e}", tol);

    start = std::chrono::steady_clock::now();

    core::CSRMatrix m(Nodes);
    std::vector<double> outDegree(Nodes, 0.0);

    for (auto& [id, document] : NodeToDocument) {
        for (const std::string& link : document.forwardLinks) {
            auto it = linkToNode.find(link);
            if (it != linkToNode.end()) {
                m.AddEdge(static_cast<int>(it->second), static_cast<int>(id), 1.0);
            } else {
                throw std::runtime_error("Link not found in linkToNode map: " + link);
            }
        }

        outDegree[id] = static_cast<double>(document.forwardLinks.size());
    }

    m.Finalize();

    size_t danglingLinks = Nodes;
    for (int i = 0; i < m.values_.size(); ++i) {
        if (outDegree[m.col_idx_[i]] > 0) {
            m.values_[i] /= outDegree[m.col_idx_[i]];
            danglingLinks--;
        }
    }
    
    end = std::chrono::steady_clock::now();
    auto csrMatrixDuration = (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    spdlog::info("Finished CSR matrix building process. Dangling links: {}, Time taken: {} ms", danglingLinks, csrMatrixDuration);
    spdlog::info("Performing page rank....");

    start = std::chrono::steady_clock::now();

    pagerank::PerformPageRank(m, Nodes);

    end = std::chrono::steady_clock::now();
    auto duration = (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    spdlog::info("Finished pagerank in: {} ms", duration);
    spdlog::info("Total time taken: {} ms", (duration + csrMatrixDuration + processDuration));
}
