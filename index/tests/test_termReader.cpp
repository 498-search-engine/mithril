#include "DocumentMapReader.h"
#include "PositionIndex.h"
#include "TermDictionary.h"
#include "TermReader.h"
#include "core/mem_map_file.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <cmath>

using Clock = std::chrono::high_resolution_clock;
using MsBetween = std::chrono::duration<double, std::milli>;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> <term>" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];
    std::string term = argv[2];

    try {
        std::cout << "Starting program" << std::endl;

        auto t0 = Clock::now();
        std::cout << "Loading document map from " << index_dir << std::endl;
        mithril::DocumentMapReader doc_reader(index_dir);
        auto t1 = Clock::now();

        std::chrono::duration<double, std::milli> doc_time = t1 - t0;
        double doc_ms = std::ceil(doc_time.count() * 100.0) / 100.0;
        std::cout << "Loaded document map with " << doc_reader.documentCount() << " documents in " << std::fixed
                  << std::setprecision(2) << doc_ms << "ms" << std::endl;

        std::cout << "Loading position index from " << index_dir << std::endl;
        auto t2 = Clock::now();
        mithril::PositionIndex position_index(index_dir);
        auto t3 = Clock::now();

        std::chrono::duration<double, std::milli> pos_time = t3 - t2;
        double pos_ms = std::ceil(pos_time.count() * 100.0) / 100.0;
        std::cout << "Loaded position index in " << std::fixed << std::setprecision(2) << doc_ms << "ms" << std::endl;

        std::cout << "Loading term dictionary from " << index_dir << std::endl;
        auto t4 = Clock::now();
        mithril::TermDictionary term_dict(index_dir);
        auto t5 = Clock::now();

        std::chrono::duration<double, std::milli> dic_time = t5 - t4;
        double dic_ms = std::ceil(dic_time.count() * 100.0) / 100.0;
        std::cout << "Loaded term dictionary in " << std::fixed << std::setprecision(2) << dic_ms << "ms" << std::endl;

        std::cout << "Memory mapping index file" << std::endl;
        core::MemMapFile index_file(index_dir + "/final_index.data");

        std::cout << "Creating TermReader for term '" << term << "'" << std::endl;
        auto t6 = Clock::now();
        mithril::TermReader term_reader(index_dir, term, index_file, term_dict, position_index);
        auto t7 = Clock::now();

        std::chrono::duration<double, std::milli> read_time = t7 - t6;
        double read_ms = std::ceil(read_time.count() * 100.0) / 100.0;
        std::cout << "Created TermReader in " << std::fixed << std::setprecision(2) << read_ms << "ms" << std::endl;

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
