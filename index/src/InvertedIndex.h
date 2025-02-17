#ifndef INDEX_INVERTEDINDEX_H
#define INDEX_INVERTEDINDEX_H

#include "TermStore.h"
#include "data/Document.h"

#include <future>

namespace mithril {

using Document = data::Document;
using docid_t = data::docid_t;

class IndexBuilder {
public:
    explicit IndexBuilder(const std::string& output_dir, size_t num_threads = std::thread::hardware_concurrency());
    ~IndexBuilder();

    void add_document(const std::string& words_path, const std::string& links_path); // remnant
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
    static constexpr size_t MAX_BLOCK_SIZE = 128 * 1024 * 1024;  // 64 MB

    // core methods
    std::future<void> flush_block();
    void merge_blocks();
    void save_document_map();
    std::string block_path(int block_num) const;
    void process_document(const Document& doc);

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

class VByteCodec {
public:
    static void encode(uint32_t value, std::ostream& out) {
        while (value >= 128) {
            out.put((value & 127) | 128);
            if (!out)
                throw std::runtime_error("Failed to write VByte");
            value >>= 7;
        }
        out.put(value);
        if (!out)
            throw std::runtime_error("Failed to write VByte");
    }

    static uint32_t decode(std::istream& in) {
        uint32_t result = 0;
        uint32_t shift = 0;
        uint8_t byte;
        do {
            byte = in.get();
            result |= (byte & 127) << shift;
            shift += 7;
        } while (byte & 128);
        return result;
    }
};

}  // namespace mithril

#endif  // INDEX_INVERTEDINDEX_H