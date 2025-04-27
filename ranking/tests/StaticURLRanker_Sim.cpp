#include "core/config.h"
#include "StaticRanker.h"

#include <fstream>
#include <iostream>
#include <set>
#include <spdlog/spdlog.h>
#include "core/pair.h"

using namespace mithril;

int main() {
    spdlog::set_level(spdlog::level::level_enum::debug); 

    core::Config config = core::Config("tests.conf");
    std::string inFilePath = std::string(config.GetString("static_ranker_in_file").Cstr());
    std::ifstream inFile(inFilePath);

    std::set<core::Pair<double, std::string>> urlRankingSet;

    for (std::string line; std::getline(inFile, line);) {
        // Empty line
        if (line.size() < 2) {
            continue;
        }
        // Comments
        if (*line.begin() == '/') {
            continue;
        }

        std::string url;
        for (char c : line) {
            if (c == ' ') {
                break;
            }
            url += c;
        }

        urlRankingSet.insert({ranking::GetUrlStaticRank(url), url});
    }

    for (const auto& pair : urlRankingSet) {
        std::cout << pair.second << ": " << pair.first << std::endl;
    }
}