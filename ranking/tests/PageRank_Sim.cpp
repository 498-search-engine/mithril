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

void WriteBackToFile() {
    spdlog::info("Writing to human readable output file {}...", OutputFile);

    std::ofstream outFile;
    outFile.open(OutputFile);

    std::vector<double>& scores = *mithril::pagerank::Results;
    std::vector<size_t> idx(mithril::pagerank::DocumentCount);
    std::iota(idx.begin(), idx.end(), 0);

    stable_sort(idx.begin(), idx.end(), [&scores](size_t i1, size_t i2) {
        auto it1 = mithril::pagerank::DocumentToNode->find(static_cast<mithril::data::docid_t>(i1));
        auto it2 = mithril::pagerank::DocumentToNode->find(static_cast<mithril::data::docid_t>(i2));

        double j1 = it1 == mithril::pagerank::DocumentToNode->end() ? 0 : scores[it1->second];
        double j2 = it2 == mithril::pagerank::DocumentToNode->end() ? 0 : scores[it2->second];
        return j1 < j2;
    });

    for (size_t i = 0; i < idx.size(); ++i) {
        int j = static_cast<int>(idx[i]);
        outFile << mithril::pagerank::ProcessLink((*mithril::pagerank::NodeToDocument)[j].url)
                << " (docid: " << (*mithril::pagerank::NodeToDocument)[j].id
                << "): " << (*mithril::pagerank::StandardizedResults)[idx[i]] << " (" << scores[idx[i]] << ")"
                << std::endl;
    }

    outFile.close();

    spdlog::info("Finished writing to file: {}", OutputFile);
}

}  // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    mithril::pagerank::PerformPageRank();

    if (WriteToFile) {
        WriteBackToFile();
    }

    mithril::pagerank::Cleanup();

    return 0;
}