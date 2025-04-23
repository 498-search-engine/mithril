#include "DocumentMapReader.h"
#include "PositionIndex.h"
#include "TermDictionary.h"
#include "TermOR.h"
#include "TermReader.h"
#include "core/mem_map_file.h"

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

        // Create term dict
        core::MemMapFile index_file(index_dir + "/final_index.data");
        mithril::TermDictionary term_dict(index_dir);
        mithril::PositionIndex position_index(index_dir);

        // Create readers for each term
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
        for (int i = 2; i < argc; ++i) {
            std::cout << "Creating TermReader for term '" << argv[i] << "'" << std::endl;
            readers.push_back(
                std::make_unique<mithril::TermReader>(index_dir, argv[i], index_file, term_dict, position_index));
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

        int displayed_count = 0;
        int total_count = 0;
        const int MAX_DOCS = 10;

        // Iterate through matching documents
        while (or_reader.hasNext()) {
            mithril::data::docid_t doc_id = or_reader.currentDocID();
            total_count++;

            // Only display up to MAX_DOCS
            if (displayed_count < MAX_DOCS) {
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
                displayed_count++;
            }

            or_reader.moveNext();
        }

        if (displayed_count < total_count) {
            std::cout << "... and " << (total_count - displayed_count) << " more documents with these terms." << std::endl;
        }
        
        std::cout << "Total documents found: " << total_count << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
