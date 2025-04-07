#include "PageRankReader.h"
#include "data/Document.h"
#include <iostream>
#include <spdlog/spdlog.h>

int main() {
    spdlog::info("Reading data...");
    mithril::pagerank::PageRankReader reader;
    spdlog::info("Read data. Max docid: {}", reader.size_);

    mithril::data::docid_t docid;
    
    while (true) {
        std::cout << "Enter docid: ";
        std::cin >> docid;

        if (docid >= reader.size_) {
            std::cout << "Invalid docid. Exiting..." << std::endl;
            break;
        }

        std::cout << reader.GetDocumentPageRank(docid) << std::endl;
    }


}