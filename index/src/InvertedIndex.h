#ifndef INDEX_INVERTEDINDEX_H
#define INDEX_INVERTEDINDEX_H

#include "TermStore.h"
#include "TextPreprocessor.h"
#include "data/Document.h"
#include "ranking/PageRankReader.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mithril {

// Type aliases
using Document = data::Document;
using docid_t = data::docid_t;

// Constants
constexpr size_t DEFAULT_MAX_TERMS_PER_BLOCK = 750000;
constexpr size_t DEFAULT_MERGE_FACTOR = 16;

struct DocumentMetadata {
    data::docid_t id;
    std::string url;
    std::vector<std::string> title;
    uint32_t body_length{0};
    uint32_t title_length{0};
    uint32_t url_length{0};
    uint32_t desc_length{0};
    float pagerank_score{0.0F};
};

struct IndexStatistics {
    uint32_t doc_count{0};
    uint64_t total_title_length{0};
    uint64_t total_body_length{0};
    uint64_t total_url_length{0};
    uint64_t total_desc_length{0};

    uint64_t getFieldTotalLength(FieldType field) const {
        switch (field) {
        case FieldType::BODY:
            return total_body_length;
        case FieldType::TITLE:
            return total_title_length;
        case FieldType::URL:
            return total_url_length;
        case FieldType::DESC:
            return total_desc_length;
        default:
            return 0;
        }
    }

    double getFieldAvgLength(FieldType field) const {
        if (doc_count == 0)
            return 0.0;
        return static_cast<double>(getFieldTotalLength(field)) / doc_count;
    }
};

class IndexBuilder {
public:
    explicit IndexBuilder(const std::string& output_dir,
                          size_t num_threads = std::thread::hardware_concurrency() * 3 / 2,
                          size_t max_terms_per_block = DEFAULT_MAX_TERMS_PER_BLOCK);

    ~IndexBuilder();

    void add_document(const std::string& doc_path);
    void finalize();
    void save_index_stats();

private:
    // Page rank reader
    // pagerank::PageRankReader pagerank_reader_;

    // Doc Metadata Storage
    std::vector<DocumentMetadata> document_metadata_;
    std::unordered_map<std::string, uint32_t> url_to_id_;
    std::mutex document_mutex_;

    // In-Memory Block State
    Dictionary dictionary_;
    size_t current_block_term_count_{0};
    int block_count_{0};
    std::mutex block_mutex_;

    // Config
    const std::string output_dir_;
    const size_t max_terms_per_block_;
    static constexpr size_t MERGE_FACTOR = DEFAULT_MERGE_FACTOR;

    // Core Indexing Methods
    std::future<void> flush_block();
    std::string merge_block_subset(const std::vector<std::string>& block_paths,
                                   size_t start_idx,
                                   size_t end_idx,
                                   int tier_num,
                                   bool is_final_output);
    void merge_blocks_tiered();
    void save_document_map();
    void create_term_dictionary();
    void process_document(Document doc);
    bool should_flush();

    std::string block_path(int block_num) const;
    std::string join_title(const std::vector<std::string>& title_words);

    // Thread Pool
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_{false};
    std::atomic<int> active_tasks_{0};
    void worker_thread();

    // size_t current_block_size_{0};
    IndexStatistics stats_;
    std::mutex stats_mutex_;
};

}  // namespace mithril

#endif  // INDEX_INVERTEDINDEX_H
