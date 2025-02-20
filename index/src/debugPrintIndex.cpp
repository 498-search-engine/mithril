#include "InvertedIndex.h"

#include <fstream>
#include <iostream>
#include <vector>

using namespace mithril;

void print_document_map(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open document map: " << path << '\n';
        return;
    }

    uint32_t num_docs;
    in.read(reinterpret_cast<char*>(&num_docs), sizeof(num_docs));
    std::cout << "Documents (" << num_docs << "):\n";

    for (uint32_t i = 0; i < num_docs; i++) {
        uint32_t doc_id, url_len, title_len;
        in.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
        in.read(reinterpret_cast<char*>(&url_len), sizeof(url_len));

        std::string url(url_len, '\0');
        in.read(&url[0], url_len);

        in.read(reinterpret_cast<char*>(&title_len), sizeof(title_len));
        std::string title(title_len, '\0');
        in.read(&title[0], title_len);

        std::cout << doc_id << " -> " << url << " (" << title << ")\n";
    }
}

void print_index(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open index: " << path << '\n';
        return;
    }

    uint32_t total_terms;
    in.read(reinterpret_cast<char*>(&total_terms), sizeof(total_terms));
    std::cout << "\nTerms (" << total_terms << "):\n";

    for (uint32_t i = 0; i < total_terms; i++) {
        // Read term
        uint32_t term_len;
        in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));

        std::string term(term_len, '\0');
        in.read(&term[0], term_len);

        // Read posting count
        uint32_t postings_size;
        in.read(reinterpret_cast<char*>(&postings_size), sizeof(postings_size));

        // Read compressed postings
        std::cout << term << " -> ";
        uint32_t last_doc_id = 0;
        for (uint32_t j = 0; j < postings_size; j++) {
            if (j > 0)
                std::cout << ", ";

            // Decode delta-encoded doc_id
            uint32_t doc_id_delta = VByteCodec::decode(in);
            uint32_t doc_id = last_doc_id + doc_id_delta;
            last_doc_id = doc_id;

            // Decode frequency
            uint32_t freq = VByteCodec::decode(in);

            std::cout << doc_id << ":" << freq;
        }
        std::cout << '\n';
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <index_directory>\n";
        return 1;
    }

    std::string dir = argv[1];
    print_document_map(dir + "/document_map.bin");
    print_index(dir + "/final_index.bin");
    return 0;
}