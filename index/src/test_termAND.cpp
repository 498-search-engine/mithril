#include "DocumentMapReader.h"
#include "TermAND.h"
#include "TermReader.h"
#include "TermDictionary.h"

#include <iostream>
#include <memory>
#include <vector>

bool check_phrase_positions(const std::vector<std::unique_ptr<mithril::TermReader>>& readers,
                            mithril::data::docid_t doc_id,
                            int max_distance = 5) {  // Allow terms to be up to 5 positions apart
    if (readers.empty())
        return false;

    // Get positions for the first term
    readers[0]->seekToDocID(doc_id);
    if (!readers[0]->hasPositions())
        return false;

    std::vector<uint16_t> positions = readers[0]->currentPositions();

    // Debug output
    // std::cout << "Term '" << readers[0]->getTerm() << "' has " << positions.size() << " positions in doc " << doc_id
    //           << std::endl;

    // For each position of the first term, check if remaining terms follow within proximity
    for (uint32_t start_pos : positions) {
        bool match = true;
        uint32_t last_pos = start_pos;

        // Check each subsequent term
        for (size_t i = 1; i < readers.size() && match; i++) {
            readers[i]->seekToDocID(doc_id);
            if (!readers[i]->hasPositions()) {
                match = false;
                break;
            }

            // Get positions for this term
            std::vector<uint16_t> term_positions = readers[i]->currentPositions();

            // std::cout << "  Term '" << readers[i]->getTerm() << "' has " << term_positions.size()
            //           << " positions in doc " << doc_id << std::endl;

            // Find the first position that comes after last_pos within max_distance
            bool found_match = false;
            for (uint32_t pos : term_positions) {
                if (pos > last_pos && (pos - last_pos) <= max_distance) {
                    last_pos = pos;
                    found_match = true;
                    break;
                }
            }

            if (!found_match) {
                match = false;
            }
        }

        if (match)
            return true;
    }

    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> <term1> <term2> [term3...] [--phrase]" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];

    bool phrase_mode = (argc > 3 && std::string(argv[argc - 1]) == "--phrase");

    try {
        // Make term dict
        mithril::TermDictionary term_dict(index_dir);

        // Create readers for each term
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> isr_readers;
        std::vector<std::unique_ptr<mithril::TermReader>> term_readers;

        for (int i = 2; i < argc - (phrase_mode ? 1 : 0); ++i) {
            // Create TermReader for position checking
            term_readers.push_back(std::make_unique<mithril::TermReader>(index_dir, argv[i], term_dict));

            // Create another TermReader for the AND operation
            isr_readers.push_back(std::make_unique<mithril::TermReader>(index_dir, argv[i], term_dict));
        }

        mithril::TermAND and_reader(std::move(isr_readers));

        // Load document map for retrieving document info
        mithril::DocumentMapReader doc_reader(index_dir);

        std::cout << "Documents containing ALL terms: ";
        for (int i = 2; i < argc - (phrase_mode ? 1 : 0); ++i) {
            std::cout << "'" << argv[i] << "' ";
        }
        std::cout << std::endl << "-------------------------------" << std::endl;

        int count = 0;
        const int MAX_DOCS = 10;

        // Iterate through matching documents
        while (and_reader.hasNext() && count < MAX_DOCS) {
            mithril::data::docid_t doc_id = and_reader.currentDocID();

            bool should_display = true;
            if (phrase_mode) {
                // Only display if terms form a phrase
                should_display = check_phrase_positions(term_readers, doc_id);
            }

            if (should_display) {
                auto doc_opt = doc_reader.getDocument(doc_id);
                if (doc_opt) {
                    std::cout << "Document ID: " << doc_id << std::endl;
                    std::cout << "  URL: " << doc_opt->url << std::endl;
                    std::cout << "  Title: ";
                    for (const auto& word : doc_opt->title) {
                        std::cout << word << " ";
                    }
                    std::cout << std::endl << std::endl;
                    count++;
                }
            }

            and_reader.moveNext();
        }

        if (and_reader.hasNext()) {
            std::cout << "... and more documents with these terms." << std::endl;
        } else if (count == 0) {
            std::cout << "No documents found containing all terms." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
