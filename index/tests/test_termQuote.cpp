#include "DocumentMapReader.h"
#include "TermDictionary.h"
#include "TermQuote.h"
#include "PositionIndex.h"

#include <iostream>
// #include <spdlog/spdlog.h>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> <term> |" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];
    std::vector<std::string> quote;
    std::string original_quote;
    for (int i = 2; i < argc; i++) {
        quote.emplace_back(argv[i]);
        original_quote += std::string(argv[i]) + " ";
    }
    original_quote.pop_back();

    mithril::TermDictionary term_dict(index_dir);

    try {
        std::cout << "Starting program" << std::endl;

        std::cout << "Loading document map from " << index_dir << std::endl;
        mithril::DocumentMapReader doc_reader(index_dir);
        std::cout << "Loaded document map with " << doc_reader.documentCount() << " documents." << std::endl;
    
        std::cout << "Loading position index from " << index_dir << std::endl;
        mithril::PositionIndex position_index(index_dir);
        std::cout << "Loaded position index." << std::endl;

        std::cout << "Creating TermQuote for quote '" << original_quote << "'" << std::endl;
        mithril::TermQuote term_quote(doc_reader, index_dir, quote, term_dict, position_index);

        std::cout << "Searching for quote: \"" << original_quote << "\"" << std::endl;

        if (!term_quote.hasNext()) {
            std::cout << "Quote not found in the index." << std::endl;
            return 0;
        }

        std::cout << "Documents containing the term:" << std::endl;
        std::cout << "-------------------------------" << std::endl;

        int count = 0;
        const int MAX_DOCS = 10;

        while (term_quote.hasNext() && count < MAX_DOCS) {
            mithril::data::docid_t doc_id = term_quote.currentDocID();

            std::cout << "Document ID: " << doc_id << std::endl;

            auto doc_opt = doc_reader.getDocument(doc_id);
            if (doc_opt) {
                std::cout << "  URL: " << doc_opt->url << std::endl;
                std::cout << "  Title: ";
                for (const auto& word : doc_opt->title) {
                    std::cout << word << " ";
                }
                std::cout << std::endl;
            }

            term_quote.moveNext();
            count++;
        }

        if (term_quote.hasNext()) {
            std::cout << "... and more documents with this quote." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
