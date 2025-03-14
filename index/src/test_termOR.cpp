#include "DocumentMapReader.h"
#include "TermOR.h"
#include "TermReader.h"

#include <iostream>
#include <memory>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> <term1> <term2> [term3...]" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];

    try {
        std::cout << "Starting program" << std::endl;

        // Create readers for each term
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
        for (int i = 2; i < argc; ++i) {
            std::cout << "Creating TermReader for term '" << argv[i] << "'" << std::endl;
            readers.push_back(std::make_unique<mithril::TermReader>(index_dir, argv[i]));
        }

        // Create the OR constraint
        std::cout << "Creating TermOR for " << readers.size() << " terms" << std::endl;
        mithril::TermOR or_reader(std::move(readers));

        // Load document map for retrieving document info
        std::cout << "Loading document map from " << index_dir << std::endl;
        mithril::DocumentMapReader doc_reader(index_dir);

        std::cout << "Documents containing ANY of the terms: ";
        for (int i = 2; i < argc; ++i) {
            std::cout << "'" << argv[i] << "' ";
        }
        std::cout << std::endl << "-------------------------------" << std::endl;

        int count = 0;
        const int MAX_DOCS = 5;

        // Iterate through matching documents
        while (or_reader.hasNext() && count < MAX_DOCS) {
            mithril::data::docid_t doc_id = or_reader.currentDocID();

            std::cout << "Document ID: " << doc_id << std::endl;

            auto doc_opt = doc_reader.getDocument(doc_id);
            if (doc_opt) {
                std::cout << "  URL: " << doc_opt->url << std::endl;
                std::cout << "  Title: ";
                for (const auto& word : doc_opt->title) {
                    std::cout << word << " ";
                }
                std::cout << std::endl << std::endl;
            }

            or_reader.moveNext();
            count++;
        }

        if (or_reader.hasNext()) {
            std::cout << "... and more documents with these terms." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
