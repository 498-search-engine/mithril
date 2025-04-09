#include "DocumentMapReader.h"
#include "TermReader.h"
#include "TermDictionary.h"

#include <iomanip>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> <term>" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];
    std::string term = argv[2];

    mithril::TermDictionary term_dict(index_dir);

    try {
        std::cout << "Starting program" << std::endl;

        std::cout << "Loading document map from " << index_dir << std::endl;
        mithril::DocumentMapReader doc_reader(index_dir);
        std::cout << "Loaded document map with " << doc_reader.documentCount() << " documents." << std::endl;

        std::cout << "Creating TermReader for term '" << term << "'" << std::endl;
        mithril::TermReader term_reader(index_dir, term, term_dict);

        std::cout << "Searching for term: \"" << term << "\"" << std::endl;

        if (!term_reader.hasNext()) {
            std::cout << "Term not found in the index." << std::endl;
            return 0;
        }

        std::cout << "Documents containing the term:" << std::endl;
        std::cout << "-------------------------------" << std::endl;

        int count = 0;
        const int MAX_DOCS = 10;

        while (term_reader.hasNext() && count < MAX_DOCS) {
            mithril::data::docid_t doc_id = term_reader.currentDocID();
            uint32_t frequency = term_reader.currentFrequency();

            std::cout << "Document ID: " << doc_id << " (appears " << frequency << " times)" << std::endl;

            auto doc_opt = doc_reader.getDocument(doc_id);
            if (doc_opt) {
                std::cout << "  URL: " << doc_opt->url << std::endl;
                std::cout << "  Title: ";
                for (const auto& word : doc_opt->title) {
                    std::cout << word << " ";
                }
                std::cout << std::endl;
            }

            if (term_reader.hasPositions()) {
                auto positions = term_reader.currentPositions();
                std::cout << "  Positions:";
                for (size_t i = 0; i < positions.size() && i < 20; ++i) {
                    std::cout << " " << positions[i];
                }
                if (positions.size() > 20) {
                    std::cout << " ... (" << positions.size() - 20 << " more)";
                }
                std::cout << std::endl;
            }

            term_reader.moveNext();
            count++;
        }

        if (term_reader.hasNext()) {
            std::cout << "... and more documents with this term." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
