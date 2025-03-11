#include "InvertedIndex.h"
#include "PostingBlock.h"

#include <algorithm>
#include <bitset>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace mithril {

// Document metadata from the document map
struct DocumentMeta {
    uint32_t id;
    std::string url;
    std::string title;
};

// Statistics for index analysis
struct IndexStats {
    size_t total_terms = 0;
    size_t total_postings = 0;
    size_t total_positions = 0;
    size_t total_position_bytes = 0;
    size_t total_sync_points = 0;
    size_t total_position_sync_points = 0;
    size_t total_bytes = 0;

    // Term frequency distribution
    std::vector<size_t> term_freq_dist;

    // Position deltas for compression analysis
    std::vector<uint32_t> position_deltas;

    // Term statistics
    struct TermStats {
        std::string term;
        size_t doc_freq = 0;
        size_t total_term_freq = 0;
        size_t positions_size = 0;

        bool operator<(const TermStats& other) const {
            return doc_freq > other.doc_freq;  // For descending sort by doc frequency
        }
    };
    std::vector<TermStats> term_stats;
};

// Utility to format byte sizes with suffixes (KB, MB, etc.)
std::string format_size(size_t bytes) {
    const char* suffixes[] = {"bytes", "KB", "MB", "GB", "TB"};
    int suffix_idx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024 && suffix_idx < 4) {
        size /= 1024;
        suffix_idx++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << suffixes[suffix_idx];
    return ss.str();
}

class IndexDebugger {
public:
    explicit IndexDebugger(const std::string& index_dir, bool verbose = false)
        : index_dir_(index_dir), verbose_(verbose) {
        document_map_path_ = index_dir_ + "/document_map.bin";
        index_path_ = index_dir_ + "/final_index.bin";
    }

    bool load_documents() {
        if (!fs::exists(document_map_path_)) {
            std::cerr << "Document map file not found: " << document_map_path_ << std::endl;
            return false;
        }

        std::ifstream in(document_map_path_, std::ios::binary);
        if (!in) {
            std::cerr << "Failed to open document map file: " << document_map_path_ << std::endl;
            return false;
        }

        uint32_t num_docs;
        in.read(reinterpret_cast<char*>(&num_docs), sizeof(num_docs));

        documents_.reserve(num_docs);

        for (uint32_t i = 0; i < num_docs; ++i) {
            DocumentMeta doc;

            in.read(reinterpret_cast<char*>(&doc.id), sizeof(doc.id));

            uint32_t url_len;
            in.read(reinterpret_cast<char*>(&url_len), sizeof(url_len));
            doc.url.resize(url_len);
            in.read(&doc.url[0], url_len);

            uint32_t title_len;
            in.read(reinterpret_cast<char*>(&title_len), sizeof(title_len));
            doc.title.resize(title_len);
            in.read(&doc.title[0], title_len);

            documents_.push_back(doc);
            doc_id_to_idx_[doc.id] = i;
        }

        std::cout << "Documents (" << documents_.size() << "):" << std::endl;
        if (verbose_) {
            for (size_t i = 0; i < std::min<size_t>(10, documents_.size()); ++i) {
                std::cout << "  " << documents_[i].id << ": " << documents_[i].url << " - " << documents_[i].title
                          << std::endl;
            }
            if (documents_.size() > 10) {
                std::cout << "  ..." << std::endl;
            }
        }

        return true;
    }

    bool analyze_index() {
        if (!fs::exists(index_path_)) {
            std::cerr << "Index file not found: " << index_path_ << std::endl;
            return false;
        }

        std::ifstream in(index_path_, std::ios::binary);
        if (!in) {
            std::cerr << "Failed to open index file: " << index_path_ << std::endl;
            return false;
        }

        // Get file size for stats
        in.seekg(0, std::ios::end);
        stats_.total_bytes = in.tellg();
        in.seekg(0, std::ios::beg);

        // Read total term count
        uint32_t num_terms;
        in.read(reinterpret_cast<char*>(&num_terms), sizeof(num_terms));
        stats_.total_terms = num_terms;

        std::cout << "Terms (" << num_terms << "):" << std::endl;

        // Prepare output file for term list
        std::ofstream term_out(index_dir_ + "/term_list.txt");
        if (!term_out) {
            std::cerr << "Warning: Failed to create term list file" << std::endl;
        }

        // Analyze each term
        for (uint32_t term_idx = 0; term_idx < num_terms; ++term_idx) {
            // Read term
            uint32_t term_len;
            in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
            std::string term(term_len, ' ');
            in.read(&term[0], term_len);

            // Read postings size
            uint32_t postings_size;
            in.read(reinterpret_cast<char*>(&postings_size), sizeof(postings_size));

            // Read sync points
            uint32_t sync_points_size;
            in.read(reinterpret_cast<char*>(&sync_points_size), sizeof(sync_points_size));
            stats_.total_sync_points += sync_points_size;

            // Skip sync points data
            in.seekg(sync_points_size * sizeof(SyncPoint), std::ios::cur);

            // Read and decode postings
            std::vector<Posting> postings;
            postings.reserve(postings_size);

            uint32_t last_doc_id = 0;
            size_t total_freq = 0;

            for (uint32_t i = 0; i < postings_size; ++i) {
                uint32_t doc_id_delta = VByteCodec::decode(in);
                uint32_t freq = VByteCodec::decode(in);

                uint32_t doc_id = last_doc_id + doc_id_delta;
                last_doc_id = doc_id;

                Posting p{doc_id, freq, 0};  // Position offset not available in this format
                postings.push_back(p);
                total_freq += freq;
            }

            // Read positions count
            uint32_t positions_size;
            in.read(reinterpret_cast<char*>(&positions_size), sizeof(positions_size));
            stats_.total_positions += positions_size;

            // Read position sync points
            uint32_t position_sync_points_size;
            in.read(reinterpret_cast<char*>(&position_sync_points_size), sizeof(position_sync_points_size));
            stats_.total_position_sync_points += position_sync_points_size;

            // Skip position sync points data
            in.seekg(position_sync_points_size * sizeof(PositionSyncPoint), std::ios::cur);

            // Read and process position deltas for statistics
            std::vector<uint32_t> position_deltas;
            position_deltas.reserve(positions_size);

            for (uint32_t i = 0; i < positions_size; ++i) {
                uint32_t delta = VByteCodec::decode(in);
                position_deltas.push_back(delta);

                // Collect deltas for global statistics (sample if too many)
                if (stats_.position_deltas.size() < 10000 || delta > 1000 || (rand() % 100 == 0)) {
                    stats_.position_deltas.push_back(delta);
                }
            }

            // Record term statistics
            stats_.total_postings += postings_size;
            IndexStats::TermStats term_stat;
            term_stat.term = term;
            term_stat.doc_freq = postings_size;
            term_stat.total_term_freq = total_freq;
            term_stat.positions_size = positions_size;
            stats_.term_stats.push_back(term_stat);

            // Update term frequency distribution
            if (postings_size >= stats_.term_freq_dist.size()) {
                stats_.term_freq_dist.resize(postings_size + 1, 0);
            }
            stats_.term_freq_dist[postings_size]++;

            // Write term details to file
            if (term_out) {
                term_out << term << "\t" << postings_size << "\t" << total_freq << "\t" << positions_size << std::endl;
            }

            // Output term details if verbose
            if (verbose_) {
                if (term_idx < 20 || term_idx % 10000 == 0 || postings_size > 1000) {
                    std::cout << "  " << std::left << std::setw(20) << term << "docs: " << std::setw(6) << postings_size
                              << "positions: " << std::setw(8) << positions_size << std::endl;

                    // Print first few postings
                    if (postings_size > 0 && postings_size < 10) {
                        std::cout << "    Postings: ";
                        for (size_t i = 0; i < postings_size; ++i) {
                            auto& posting = postings[i];
                            std::cout << posting.doc_id << "(" << posting.freq << ") ";
                        }
                        std::cout << std::endl;
                    }
                }
            }

            // Show progress
            if (term_idx % 10000 == 0) {
                std::cout << "\rAnalyzing terms: " << term_idx << "/" << num_terms << " ("
                          << (term_idx * 100 / num_terms) << "%)" << std::flush;
            }
        }

        std::cout << "\rAnalyzing terms: " << num_terms << "/" << num_terms << " (100%)" << std::endl;
        return true;
    }

    void print_statistics() {
        // Sort term stats by document frequency
        std::sort(stats_.term_stats.begin(), stats_.term_stats.end());

        // Calculate position delta statistics
        std::cout << "\nPosition Storage Statistics:" << std::endl;
        std::cout << "  Total terms: " << stats_.total_terms << std::endl;
        std::cout << "  Total positions: " << stats_.total_positions << std::endl;

        // Estimate position bytes using VByte encoding formula
        stats_.total_position_bytes = 0;
        for (uint32_t delta : stats_.position_deltas) {
            stats_.total_position_bytes += VByteCodec::max_bytes_needed(delta);
        }
        size_t raw_position_bytes = stats_.total_positions * sizeof(uint32_t);
        double compression_ratio = static_cast<double>(raw_position_bytes) / stats_.total_position_bytes;

        std::cout << "  Total position bytes: " << format_size(stats_.total_position_bytes) << std::endl;
        std::cout << "  Total position sync points: " << stats_.total_position_sync_points << std::endl;

        // Calculate and print position delta statistics
        if (!stats_.position_deltas.empty()) {
            double avg_delta = std::accumulate(stats_.position_deltas.begin(), stats_.position_deltas.end(), 0.0) /
                               stats_.position_deltas.size();
            auto max_delta = *std::max_element(stats_.position_deltas.begin(), stats_.position_deltas.end());

            std::cout << "  Average delta between positions: " << std::fixed << std::setprecision(2) << avg_delta
                      << std::endl;
            std::cout << "  Largest delta: " << max_delta << std::endl;
            std::cout << "  Compression ratio: " << std::fixed << std::setprecision(2) << compression_ratio << "x"
                      << std::endl;
        }

        // Print top terms by document frequency
        std::cout << "\nTop 20 terms by document frequency:" << std::endl;
        std::cout << std::left << std::setw(20) << "Term" << std::setw(12) << "Doc Freq" << std::setw(12) << "Term Freq"
                  << "Positions" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        for (size_t i = 0; i < std::min<size_t>(20, stats_.term_stats.size()); ++i) {
            const auto& term_stat = stats_.term_stats[i];
            std::cout << std::left << std::setw(20) << term_stat.term << std::setw(12) << term_stat.doc_freq
                      << std::setw(12) << term_stat.total_term_freq << term_stat.positions_size << std::endl;
        }

        // Print distribution of term frequencies
        std::cout << "\nTerm frequency distribution:" << std::endl;
        std::cout << "Documents | Terms" << std::endl;
        std::cout << "----------+------" << std::endl;

        // Group distributions for better presentation
        std::map<std::string, size_t> grouped_dist;
        size_t sum = 0;

        for (size_t i = 0; i < stats_.term_freq_dist.size(); ++i) {
            if (stats_.term_freq_dist[i] > 0) {
                sum += stats_.term_freq_dist[i];

                std::string range;
                if (i == 1)
                    range = "1";
                else if (i <= 10)
                    range = "2-10";
                else if (i <= 100)
                    range = "11-100";
                else if (i <= 1000)
                    range = "101-1,000";
                else if (i <= 10000)
                    range = "1,001-10,000";
                else
                    range = ">10,000";

                grouped_dist[range] += stats_.term_freq_dist[i];
            }
        }

        for (const auto& pair : grouped_dist) {
            std::cout << std::left << std::setw(10) << pair.first << "| " << pair.second << " (" << std::fixed
                      << std::setprecision(2) << (100.0 * pair.second / sum) << "%)" << std::endl;
        }

        // Print overall index size information
        std::cout << "\nIndex Size Information:" << std::endl;
        std::cout << "  Total index size: " << format_size(stats_.total_bytes) << std::endl;
        std::cout << "  Average bytes per document: "
                  << format_size(stats_.total_bytes / (documents_.empty() ? 1 : documents_.size())) << std::endl;
        std::cout << "  Average bytes per term: "
                  << format_size(stats_.total_bytes / (stats_.total_terms ? stats_.total_terms : 1)) << std::endl;
    }

    void export_detailed_stats() {
        // Export detailed posting lists for further analysis
        std::ofstream posting_out(index_dir_ + "/detailed_postings.txt");
        if (!posting_out) {
            std::cerr << "Warning: Failed to create detailed postings file" << std::endl;
            return;
        }

        std::ifstream in(index_path_, std::ios::binary);
        if (!in) {
            std::cerr << "Failed to reopen index file for detailed export" << std::endl;
            return;
        }

        // Skip header
        uint32_t num_terms;
        in.read(reinterpret_cast<char*>(&num_terms), sizeof(num_terms));

        std::cout << "\nExporting detailed posting lists to " << index_dir_ << "/detailed_postings.txt" << std::endl;

        // Limit the detailed export to the top 100 terms
        size_t terms_to_export = std::min<size_t>(100, stats_.term_stats.size());
        std::unordered_set<std::string> terms_to_include;

        for (size_t i = 0; i < terms_to_export; ++i) {
            terms_to_include.insert(stats_.term_stats[i].term);
        }

        // Process each term
        for (uint32_t term_idx = 0; term_idx < num_terms; ++term_idx) {
            // Read term
            uint32_t term_len;
            in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
            std::string term(term_len, ' ');
            in.read(&term[0], term_len);

            // Read postings size
            uint32_t postings_size;
            in.read(reinterpret_cast<char*>(&postings_size), sizeof(postings_size));

            // Read sync points
            uint32_t sync_points_size;
            in.read(reinterpret_cast<char*>(&sync_points_size), sizeof(sync_points_size));

            // Skip sync points data
            in.seekg(sync_points_size * sizeof(SyncPoint), std::ios::cur);

            bool include_this_term = terms_to_include.find(term) != terms_to_include.end();

            if (include_this_term) {
                posting_out << "TERM: " << term << " (docs: " << postings_size << ")" << std::endl;

                // Read and decode postings
                std::vector<std::pair<uint32_t, uint32_t>> postings;  // doc_id, freq
                postings.reserve(postings_size);

                uint32_t last_doc_id = 0;

                for (uint32_t i = 0; i < postings_size; ++i) {
                    uint32_t doc_id_delta = VByteCodec::decode(in);
                    uint32_t freq = VByteCodec::decode(in);

                    uint32_t doc_id = last_doc_id + doc_id_delta;
                    last_doc_id = doc_id;

                    postings.emplace_back(doc_id, freq);
                }

                // Write detailed posting information
                for (size_t i = 0; i < std::min<size_t>(100, postings.size()); ++i) {
                    auto [doc_id, freq] = postings[i];
                    std::string doc_url = "unknown";
                    std::string doc_title = "unknown";

                    auto it = doc_id_to_idx_.find(doc_id);
                    if (it != doc_id_to_idx_.end() && it->second < documents_.size()) {
                        doc_url = documents_[it->second].url;
                        doc_title = documents_[it->second].title;
                    }

                    posting_out << "  " << doc_id << " (freq: " << freq << "): " << doc_url.substr(0, 50) << " - "
                                << doc_title.substr(0, 50) << std::endl;
                }

                if (postings.size() > 100) {
                    posting_out << "  ... and " << (postings.size() - 100) << " more documents" << std::endl;
                }

                posting_out << std::endl;
            } else {
                // Skip all postings data
                for (uint32_t i = 0; i < postings_size; ++i) {
                    VByteCodec::decode(in);  // Skip doc_id delta
                    VByteCodec::decode(in);  // Skip freq
                }
            }

            // Read positions count
            uint32_t positions_size;
            in.read(reinterpret_cast<char*>(&positions_size), sizeof(positions_size));

            // Read position sync points
            uint32_t position_sync_points_size;
            in.read(reinterpret_cast<char*>(&position_sync_points_size), sizeof(position_sync_points_size));

            // Skip position sync points data
            in.seekg(position_sync_points_size * sizeof(PositionSyncPoint), std::ios::cur);

            // Skip position deltas
            for (uint32_t i = 0; i < positions_size; ++i) {
                VByteCodec::decode(in);
            }

            // Show progress
            if (term_idx % 10000 == 0) {
                std::cout << "\rExporting terms: " << term_idx << "/" << num_terms << " ("
                          << (term_idx * 100 / num_terms) << "%)" << std::flush;
            }
        }

        std::cout << "\rExporting terms: " << num_terms << "/" << num_terms << " (100%)" << std::endl;
    }

private:
    std::string index_dir_;
    std::string document_map_path_;
    std::string index_path_;
    bool verbose_;

    std::vector<DocumentMeta> documents_;
    std::unordered_map<uint32_t, size_t> doc_id_to_idx_;
    IndexStats stats_;
};

}  // namespace mithril

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> [--verbose] [--detailed]" << std::endl;
        return 1;
    }

    std::string index_dir = argv[1];
    bool verbose = false;
    bool detailed = false;

    for (int i = 2; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--detailed") {
            detailed = true;
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    mithril::IndexDebugger debugger(index_dir, verbose);

    if (!debugger.load_documents())
        return 1;
    if (!debugger.analyze_index())
        return 1;

    debugger.print_statistics();

    if (detailed)
        debugger.export_detailed_stats();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\nDebug analysis completed in " << duration.count() / 1000.0 << " seconds" << std::endl;

    return 0;
}