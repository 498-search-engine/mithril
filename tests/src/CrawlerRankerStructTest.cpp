#include "ranking/CrawlerRanker.h"
#include <fstream>
#include <iostream>

using namespace mithril;

int main() {
    std::ifstream in_file("config/CrawlerRankerURLs.txt");

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

        ranking::CrawlerRankingsStruct ranker{
            .tld = "", 
            .domainName = "",
            .extension = "",
            .urlLength = 0, 
            .parameterCount = 0,
            .pageDepth = 0, 
            .subdomainCount = 0,
            .numberInDomainName = false,
            .numberInURL = false,
            .isHttps = false
        };
    
        GetStringRankings(url, ranker);

        std::cout << url << "\n"
                << "TLD: " << ranker.tld << "\n"
                << "Domain name: " << ranker.domainName << "\n"
                << "Extension: " << ranker.extension << "\n"
                << "URL Length: " << ranker.urlLength << "\n"
                << "Param Count: " << ranker.parameterCount << "\n"
                << "Page depth: " << ranker.pageDepth << "\n"
                << "Subdomain Count: " << ranker.subdomainCount << "\n"
                << "Number in domain name: " << ranker.numberInDomainName << "\n"
                << "Number in URL: " << ranker.numberInURL << "\n"
                << "IsHttps: " << ranker.isHttps << "\n\n";
    }
}