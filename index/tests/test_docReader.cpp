#include "DocumentMapReader.h"

#include <iomanip>
#include <iostream>

void printDocumentInfo(const mithril::data::Document& doc) {
    std::cout << "Document ID: " << doc.id << std::endl;
    std::cout << "URL: " << doc.url << std::endl;
    std::cout << "Title: ";
    for (const auto& word : doc.title) {
        std::cout << word << " ";
    }
    std::cout << std::endl << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> [doc_id]" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];

    try {
        mithril::DocumentMapReader doc_reader(index_dir);
        std::cout << "Loaded document map with " << doc_reader.documentCount() << " documents." << std::endl;

        // If a specific doc_id is provided, display it
        if (argc > 2) {
            mithril::data::docid_t doc_id = std::stoul(argv[2]);
            auto doc_opt = doc_reader.getDocument(doc_id);

            if (doc_opt) {
                std::cout << "Found requested document:" << std::endl;
                printDocumentInfo(*doc_opt);
            } else {
                std::cout << "Document with ID " << doc_id << " not found." << std::endl;
            }
        } else {
            // Display first 5 documents
            std::cout << "First 5 documents:" << std::endl;
            int count = 0;
            while (doc_reader.hasNext() && count < 5) {
                printDocumentInfo(doc_reader.next());
                count++;
            }

            // Reset and find a document by URL
            doc_reader.reset();
            if (doc_reader.hasNext()) {
                auto first_doc = doc_reader.next();
                std::cout << "Looking up document by URL: " << first_doc.url << std::endl;

                auto found_id = doc_reader.lookupDocID(first_doc.url);
                if (found_id) {
                    std::cout << "Found document ID: " << *found_id << std::endl;

                    // Verify it's the same document
                    auto found_doc = doc_reader.getDocument(*found_id);
                    if (found_doc) {
                        std::cout << "Verified document:" << std::endl;
                        printDocumentInfo(*found_doc);
                    }
                } else {
                    std::cout << "URL lookup failed." << std::endl;
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
