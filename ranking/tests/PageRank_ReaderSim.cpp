#include "data/Document.h"
#include "ranking/PageRankReader.h"

#include <iostream>
#include <spdlog/spdlog.h>

int main() {
    spdlog::info("Reading data...");
    mithril::pagerank::PageRankReader reader;
    spdlog::info("Read data. Number of docid: {} | Start doc id {}", reader.size_, reader.start);

    std::string docid_str;
    mithril::data::docid_t docid;

    while (true) {
        std::cout << "Enter docid: ";
        std::cin >> docid_str;

        try {
            docid = atoi(docid_str.c_str());
        } catch (const std::exception& e) {
            std::cout << "Invalid docid. Exiting..." << std::endl;
            break;
        }

        std::cout << reader.GetDocumentPageRank(docid) << std::endl;
    }
}