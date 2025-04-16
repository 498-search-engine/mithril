#include "PageRank.h"
#include "data/Document.h"

#include <fstream>
#include <numeric>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace {
core::Config Config = core::Config("tests.conf");
const std::string InputDirectory = std::string(Config.GetString("simulation_input_index_data_folder").Cstr());
const std::string OutputFile = std::string(Config.GetString("pagerank_sim_out").Cstr());
const bool WriteToFile = Config.GetInt("write_results_to_file") != 0;
std::string UseInputDirectory = InputDirectory;

void WriteBackToFile() {
    spdlog::info("Writing to human readable output file {}...", OutputFile);

    std::ofstream outFile;
    outFile.open(OutputFile);

    std::vector<float>& scores = *mithril::pagerank::Results;
    std::vector<size_t> idx(mithril::pagerank::DocumentCount);
    std::iota(idx.begin(), idx.end(), 0);

    sort(idx.begin(), idx.end(), [&scores](size_t i1, size_t i2) {
        auto it1 = mithril::pagerank::DocumentToNode->find(static_cast<mithril::data::docid_t>(i1));
        auto it2 = mithril::pagerank::DocumentToNode->find(static_cast<mithril::data::docid_t>(i2));

        double j1 = it1 == mithril::pagerank::DocumentToNode->end() ? 0 : scores[it1->second];
        double j2 = it2 == mithril::pagerank::DocumentToNode->end() ? 0 : scores[it2->second];
        return j1 < j2;
    });

    for (size_t i = 0; i < idx.size(); ++i) {
        size_t docID = idx[i];
        auto it1 = mithril::pagerank::DocumentToNode->find(static_cast<mithril::data::docid_t>(docID));
        if (it1 == mithril::pagerank::DocumentToNode->end()) {
            outFile << "docid " << docID << " has no information\n";
            continue;
        }

        int node = it1->second;

        outFile << mithril::pagerank::ProcessLink((*mithril::pagerank::NodeToDocument)[node].url)
                << " (docid: " << (*mithril::pagerank::NodeToDocument)[node].id
                << "): " << (*mithril::pagerank::StandardizedResults)[node] << " (" << scores[node] << ")" << std::endl;
    }

    outFile.close();

    spdlog::info("Finished writing to file: {}", OutputFile);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        UseInputDirectory = std::string(argv[1]);
    }

    spdlog::info("Using input crawler data from: {}", UseInputDirectory);
    mithril::pagerank::PerformPageRank(UseInputDirectory);

    if (WriteToFile) {
        WriteBackToFile();
    }

    return 0;
}