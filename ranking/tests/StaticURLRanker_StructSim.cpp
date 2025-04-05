#include "core/config.h"
#include "StaticRanker.h"

#include <fstream>
#include <iostream>

using namespace mithril;

int main() {
    core::Config config = core::Config("tests.conf");
    std::string inFilePath = std::string(config.GetString("static_ranker_in_file").Cstr());
    std::ifstream inFile(inFilePath);

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

        ranking::StaticRankingsStruct ranker{};
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