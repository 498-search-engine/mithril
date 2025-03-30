#include "ranking/CrawlerRanker.h"
#include <fstream>
#include <set>
#include <iostream>

using namespace mithril;

int main() {
    std::ifstream in_file("config/CrawlerRankerURLs.txt");
    std::set<std::pair<int, std::string>> url_ranking_set;

    for (std::string line; std::getline(in_file, line);) {
        // Empty line
        if (line.size() < 2) continue;
        // Comments
        if (*line.begin() == '/') continue;

        std::string url;
        for (char c : line) {
            if (c == ' ') break;
            url += c;
        }

        url_ranking_set.insert({ranking::GetUrlRank(url), url});
    }

    for (const auto &pair : url_ranking_set) {
        std::cout << pair.second << ": " << pair.first << std::endl;
    }
}