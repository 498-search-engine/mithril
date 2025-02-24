#include "ranking/CrawlerRanker.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <utility>
#include <vector>

int main() {
    std::vector<std::string> realURLs;

    std::ifstream file("ranking/src/_Links.txt");
    std::string line;
    while (std::getline(file, line)) {
        realURLs.push_back(line);
    }

    std::priority_queue<std::pair<uint32_t, std::string>> crawlerQueue;
    for (const std::string& url : realURLs) {
        uint32_t ranking = mithril::ranking::GetUrlRank(url);
        crawlerQueue.push({ranking, url});
    }

    size_t i = 1;
    while (!crawlerQueue.empty()) {
        const auto& p = crawlerQueue.top();

        std::cout << i << ". " << p.second << " - " << p.first << std::endl;
        crawlerQueue.pop();

        i++;
    }
}
