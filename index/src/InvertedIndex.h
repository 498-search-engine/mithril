#ifndef INDEX_INVERTEDINDEX_H
#define INDEX_INVERTEDINDEX_H

#include "TermStore.h"
#include "data/Document.h"

#include <future>
#include <iostream>
#include <queue>

namespace mithril {

using Document = data::Document;
using docid_t = data::docid_t;

class IndexBuilder {
public:
    explicit IndexBuilder(const std::string& output_dir,
                          size_t num_threads = std::thread::hardware_concurrency() * 3 / 2);
    ~IndexBuilder();

    void add_document(const std::string& words_path, const std::string& links_path);  // remnant
    void add_document(const std::string& doc_path);
    // void add_document(const Document& doc);
    void finalize();

private:
    // doc
    std::vector<data::Document> documents_;
    std::unordered_map<std::string, uint32_t> url_to_id_;

    // Current block
    size_t current_block_size_{0};
    Dictionary dictionary_;
    int block_count_{0};

    // Output config
    const std::string output_dir_;
    std::string temp_dir_;
    static constexpr size_t MAX_BLOCK_SIZE = 512 * 1024 * 1024;  // 1 GB
    static constexpr size_t MERGE_FACTOR = 32;

    // core methods
    std::future<void> flush_block();
    void merge_blocks();
    std::string merge_block_subset(const std::vector<std::string>& block_paths,
                                   size_t start_idx,
                                   size_t end_idx,
                                   bool is_final_output = false);
    void merge_blocks_tiered();

    void save_document_map();
    std::string block_path(int block_num) const;
    void process_document(const Document& doc);
    void create_term_dictionary();

    // helpers
    void add_terms(data::docid_t doc_id, const std::unordered_map<std::string, uint32_t>& term_freqs);
    std::string join_title(const std::vector<std::string>& title_words);
    size_t estimate_memory_usage(const Document& doc);
    bool should_flush(const Document& doc);
    std::string StringVecToString(const std::vector<std::string>& vec);

    // File parsing helpers
    static bool parse_link_line(const std::string& line, std::string& url, std::string& title);
    static std::vector<std::string> read_words(const std::string& path);

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_{false};
    std::atomic<int> active_tasks_{0};
    void worker_thread();

    std::mutex block_mutex_;
    std::mutex document_mutex_;
};

}  // namespace mithril

#endif  // INDEX_INVERTEDINDEX_H
