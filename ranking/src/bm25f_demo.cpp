#include "BM25F.h"
#include "DocumentMapReader.h"
#include "TermOR.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

namespace mithril {

void print_document(const data::Document& doc) {
    std::cout << "  URL: " << doc.url << "\n";
    std::cout << "  Title: ";
    for (const auto& word : doc.title) {
        std::cout << word << " ";
    }
    std::cout << "\n";
}

void run_bm25_demo(const std::string& index_dir, const std::vector<std::string>& terms) {
    try {
        // Initialize BM25F scorer
        ranking::BM25F bm25(index_dir);

        // Create term readers
        std::vector<std::unique_ptr<TermReader>> term_readers;
        for (const auto& term : terms) {
            try {
                auto reader = std::make_unique<TermReader>(index_dir, term);
                if (reader->getDocumentCount() > 0) {
                    term_readers.push_back(std::move(reader));
                    std::cout << "Term '" << term << "' found in " << term_readers.back()->getDocumentCount()
                              << " docs\n";
                } else {
                    std::cout << "Term '" << term << "' not found in index\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading term '" << term << "': " << e.what() << "\n";
            }
        }

        if (term_readers.empty()) {
            std::cerr << "No valid terms to search\n";
            return;
        }

        // Collect all documents containing ANY term (OR logic)
        TermOR or_reader([&]() {
            std::vector<std::unique_ptr<IndexStreamReader>> readers;
            for (const auto& tr : term_readers) {
                readers.push_back(std::make_unique<TermReader>(index_dir, tr->getTerm()));
            }
            return readers;
        }());

        // Score documents
        std::vector<std::pair<data::docid_t, double>> scored_docs;
        while (or_reader.hasNext()) {
            data::docid_t doc_id = or_reader.currentDocID();
            double score = bm25.scoreForDoc(term_readers, doc_id);
            scored_docs.emplace_back(doc_id, score);
            or_reader.moveNext();
        }

        // Sort by BM25 score (descending)
        std::sort(
            scored_docs.begin(), scored_docs.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

        // Display top results
        const size_t max_results = 10;
        std::cout << "\nTop " << std::min(max_results, scored_docs.size()) << " results by BM25F score:\n";
        std::cout << "========================================\n";

        for (size_t i = 0; i < std::min(max_results, scored_docs.size()); ++i) {
            auto doc_id = scored_docs[i].first;
            auto score = scored_docs[i].second;
            auto doc_opt = bm25.getDocument(doc_id);

            std::cout << i + 1 << ". DocID: " << doc_id << " (Score: " << std::fixed << std::setprecision(3) << score
                      << ")\n";

            if (doc_opt) {
                print_document(*doc_opt);
            }
            std::cout << "----------------------------------------\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

}  // namespace mithril

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> <term1> [term2...]\n";
        return 1;
    }

    std::string index_dir = argv[1];
    std::vector<std::string> terms;
    for (int i = 2; i < argc; ++i) {
        terms.push_back(argv[i]);
    }

    std::cout << "Running BM25F ranking demo with terms: ";
    for (const auto& term : terms) {
        std::cout << "'" << term << "' ";
    }
    std::cout << "\n\n";

    mithril::run_bm25_demo(index_dir, terms);
    return 0;
}