#include "core/config.h"
#include "ranking/CrawlerRanker.h"

#include <fstream>
#include <iostream>
#include <set>
#include "core/pair.h"

using namespace mithril;

int main() {
    core::Config config = core::Config("tests.conf");
    std::string inFilePath = std::string(config.GetString("crawler_ranker_in_file").Cstr());
    std::ifstream inFile(inFilePath);

    std::set<core::Pair<int, std::string>> urlRankingSet;

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

        urlRankingSet.insert({ranking::GetUrlRank(url), url});
    }

    for (const auto& pair : urlRankingSet) {
        std::cout << pair.second << ": " << pair.first << std::endl;
    }
}