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
const std::string OutputFile = std::string(Config.GetString("pagerank_sim_out").Cstr());
}  // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    mithril::pagerank::PerformPageRank();

    std::ofstream outFile;
    outFile.open(OutputFile);

    std::vector<double> &scores = mithril::pagerank::Results;
    std::vector<size_t> idx(scores.size());
    std::iota(idx.begin(), idx.end(), 0);

    stable_sort(idx.begin(), idx.end(), [&scores](size_t i1, size_t i2) { return scores[i1] < scores[i2]; });

    for (size_t i = 0; i < idx.size(); ++i) {
        outFile << mithril::pagerank::NodeToDocument[idx[i]].url << ": " << scores[idx[i]] << std::endl;
    }

    outFile.close();

    spdlog::info("Finished writing to file: {}", OutputFile);

    return 0;
}