#include "InvertedIndex.h"

#include "PositionIndex.h"
#include "TextPreprocessor.h"
#include "Utils.h"
#include "data/Deserialize.h"
#include "data/Gzip.h"
#include "data/Reader.h"
#include "data/Writer.h"

#include <algorithm>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril {

class MappedFileReader {
    int fd_ = -1;
    void* mapped_data_ = MAP_FAILED;
    size_t size_ = 0;

public:
    MappedFileReader(const std::string& path) {
        fd_ = open(path.c_str(), O_RDONLY);
        if (fd_ == -1) {
            spdlog::error("MappedFileReader: Failed to open file '{}': {}", path, strerror(errno));
            throw std::runtime_error("Failed to open file: " + path);
        }

        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            int err = errno;
            close(fd_);
            spdlog::error("MappedFileReader: Failed to fstat file '{}': {}", path, strerror(err));
            throw std::runtime_error("Failed to get file size: " + path);
        }
        size_ = sb.st_size;

        if (size_ > 0) {
            mapped_data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
            if (mapped_data_ == MAP_FAILED) {
                int err = errno;
                close(fd_);
                spdlog::error("MappedFileReader: Failed to mmap file '{}': {}", path, strerror(err));
                throw std::runtime_error("Failed to memory map file: " + path);
            }
        } else {
            mapped_data_ = nullptr;
            // Don't close fd_ here, destructor will handle it. mmap wasn't called.
        }
    }

    ~MappedFileReader() {
        if (mapped_data_ != MAP_FAILED && mapped_data_ != nullptr) {
            if (munmap(mapped_data_, size_) == -1) {
                spdlog::error("MappedFileReader: Failed to munmap: {}", strerror(errno));
                // Continue to close fd
            }
        }
        if (fd_ != -1) {
            if (close(fd_) == -1) {
                spdlog::error("MappedFileReader: Failed to close fd: {}", strerror(errno));
            }
        }
    }

    // Disable copy/move semantics
    MappedFileReader(const MappedFileReader&) = delete;
    MappedFileReader& operator=(const MappedFileReader&) = delete;
    MappedFileReader(MappedFileReader&&) = delete;
    MappedFileReader& operator=(MappedFileReader&&) = delete;

    const char* data() const { return static_cast<const char*>(mapped_data_); }
    size_t size() const { return size_; }
    bool isValid() const { return fd_ != -1 && (size_ == 0 || mapped_data_ != MAP_FAILED); }
};


IndexBuilder::IndexBuilder(const std::string& output_dir, size_t num_threads, size_t max_terms_per_block)
    : output_dir_(output_dir),
      max_terms_per_block_(max_terms_per_block == 0 ? DEFAULT_MAX_TERMS_PER_BLOCK : max_terms_per_block) {
    spdlog::info("Initializing IndexBuilder: Output='{}', Threads={}, MaxTermsPerBlock={}",
                 output_dir_,
                 num_threads,
                 max_terms_per_block_);
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(output_dir + "/blocks");
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&IndexBuilder::worker_thread, this);
    }
}

IndexBuilder::~IndexBuilder() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void IndexBuilder::worker_thread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty())
                return;
            active_tasks_++;
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        try {
            task();
        } catch (const std::exception& e) {
            spdlog::error("Exception caught in worker thread task: {}", e.what());
        } catch (...) {
            spdlog::error("Unknown exception caught in worker thread task.");
        }

        active_tasks_--;
        condition_.notify_all();  // Notify finalize() thread potentially waiting
    }
}

bool IndexBuilder::should_flush() {
    // Flush based on number of unique terms added to the block's dictionary instead of previous mem size estimate
    // approach flush_block will re-verify state under lock.
    return current_block_term_count_ >= max_terms_per_block_;
}

namespace {  // anon namespace for helpers

std::vector<std::string> tokenizeUrl(std::string_view url) {
    std::vector<std::string> tokens;
    const char* delims = "/.-_?&=";
    size_t start = 0;
    size_t end = 0;

    // Trim leading delimiters often found in paths like "/path/"
    start = url.find_first_not_of(delims);
    if (start == std::string_view::npos) {
        return tokens;  // URL consists only of delimiters or is empty
    }

    while (start < url.length()) {
        end = url.find_first_of(delims, start);

        // If delimiter found, extract token before it
        if (end != std::string_view::npos) {
            if (end > start) {
                tokens.emplace_back(url.substr(start, end - start));
            }
            // Find next start position after the delimiter sequence
            start = url.find_first_not_of(delims, end + 1);
            if (start == std::string_view::npos)
                break;  // No more non-delimiter characters
        }
        // If no more delimiters, extract the rest of the string as the last token
        else {
            tokens.emplace_back(url.substr(start));
            break;  // End of URL
        }
    }
    return tokens;
}

void processField(const std::vector<std::string>& words,
                  FieldType field,
                  std::unordered_map<std::string, uint32_t>& term_freqs,
                  std::unordered_map<std::string, FieldPositions>& term_positions,
                  size_t& total_term_count) {
    uint16_t pos = 0;
    size_t field_idx = static_cast<size_t>(field);
    bool tracking_positions = true;

    for (const auto& word : words) {
        std::string normalized = TokenNormalizer::normalize(std::string_view(word), field);
        if (!normalized.empty()) {
            term_freqs[normalized]++;
            total_term_count++;

            if (tracking_positions) {
                if (pos < UINT16_MAX) {
                    auto& field_pos = term_positions[normalized];
                    field_pos.positions[field_idx].push_back(pos++);
                    field_pos.field_flags |= fieldTypeToFlag(field);
                } else {
                    tracking_positions = false;
                    // spdlog::warn("Field {} position overflow for doc, stopping position tracking for this field",
                    //              static_cast<int>(field));
                }
            }
        }
    }
}

}  // namespace

void IndexBuilder::process_document(Document doc) {
    if (should_flush()) {
        auto future = flush_block();
        if (future.valid()) {
            future.wait();
        }
    }

    auto task = [this, doc = std::move(doc)]() {
        const size_t estimated_unique_terms =
            doc.words.size() / 4 + doc.title.size() + doc.description.size() + doc.url.size() / 5 + 10;
        std::unordered_map<std::string, uint32_t> term_freqs;
        std::unordered_map<std::string, FieldPositions> term_positions;
        term_freqs.reserve(estimated_unique_terms);
        term_positions.reserve(estimated_unique_terms);

        size_t total_term_count = 0;

        auto url_tokens = tokenizeUrl(doc.url);
        processField(url_tokens, FieldType::URL, term_freqs, term_positions, total_term_count);
        processField(doc.title, FieldType::TITLE, term_freqs, term_positions, total_term_count);
        processField(doc.description, FieldType::DESC, term_freqs, term_positions, total_term_count);
        processField(doc.words, FieldType::BODY, term_freqs, term_positions, total_term_count);

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.doc_count++;
            stats_.total_title_length += doc.title.size();
            stats_.total_body_length += doc.words.size();
            stats_.total_url_length += url_tokens.size();
            stats_.total_desc_length += doc.description.size();
        }

        {
            std::unique_lock<std::mutex> lock(document_mutex_);
            url_to_id_[doc.url] = doc.id;
            document_metadata_.push_back({doc.id,
                                          doc.url,
                                          doc.title,
                                          static_cast<uint32_t>(doc.words.size()),
                                          static_cast<uint32_t>(doc.title.size()),
                                          static_cast<uint32_t>(url_tokens.size()),
                                          static_cast<uint32_t>(doc.description.size()),
					  0.0,
                                        // pagerank_reader_.GetDocumentPageRank(doc.id)
					});
        }

        // position indexing batching
        std::vector<std::pair<std::string, FieldPositions>> position_batch;
        position_batch.reserve(term_positions.size());
        for (auto it = term_positions.begin(); it != term_positions.end(); /* no increment */) {
            const std::string& term = it->first;
            try {
                uint32_t freq = term_freqs.at(term);
                if (PositionIndex::shouldStorePositions(term, freq, total_term_count)) {
                    position_batch.emplace_back(term, std::move(it->second));
                }
                it = term_positions.erase(it);
            } catch (const std::out_of_range& oor) {
                spdlog::critical(
                    "INVARIANT VIOLATION: Term '{}' in term_positions but not term_freqs for doc {}. OOR: {}",
                    term,
                    doc.id,
                    oor.what());
                it = term_positions.erase(it);
            }
        }

        if (!position_batch.empty()) {
            PositionIndex::addPositionsBatch(output_dir_, doc.id, std::move(position_batch));  // Move the batch
        }

        {
            std::lock_guard<std::mutex> lock(block_mutex_);
            for (const auto& [term, freq] : term_freqs) {
                auto& postings = dictionary_.get_or_create(term);
                bool term_was_new_to_block = postings.empty();
                postings.add(doc.id, freq);
                if (term_was_new_to_block) {
                    current_block_term_count_++;
                }
            }
        }
    };  // End of lambda task

    // Enqueue the processing task
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::move(task));
    }
    condition_.notify_one();
}

std::string IndexBuilder::join_title(const std::vector<std::string>& title_words) {
    if (title_words.empty())
        return "";
    std::string result = title_words[0];
    for (size_t i = 1; i < title_words.size(); ++i) {
        result += " ";
        result += title_words[i];
    }
    return result;
}

void IndexBuilder::add_document(const std::string& doc_path) {
    Document doc;
    try {
        data::FileReader file{doc_path.c_str()};
        data::GzipReader gzip{file};
        if (!data::DeserializeValue(doc, gzip)) {
            spdlog::error("Failed to deserialize document: {}", doc_path);
            return;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error reading document {}: {}", doc_path, e.what());
        return;
    }

    process_document(std::move(doc));
}

std::future<void> IndexBuilder::flush_block() {
    if (current_block_term_count_ == 0) {
        return std::future<void>();
    }

    std::vector<std::tuple<std::string, std::vector<Posting>>> sorted_terms;
    sorted_terms.reserve(current_block_term_count_);

    bool dictionary_was_empty = false;
    {
        std::lock_guard<std::mutex> lock(block_mutex_);
        if (current_block_term_count_ == 0) {
            dictionary_was_empty = true;
        } else {
            dictionary_.iterate_terms([&](const std::string& term, const PostingList& postings) {
                if (!postings.empty()) {
                    sorted_terms.emplace_back(term, postings.postings());
                }
            });

            dictionary_.clear_postings();
            current_block_term_count_ = 0;
        }
    }

    // If dictionary was actually empty when lock was acquired, or no terms with postings found
    if (dictionary_was_empty || sorted_terms.empty()) {
        // Reset block count just in case if we thought we were flushing but weren't
        // increment block_count_ only when successfully launching the async task
        return std::future<void>();
    }

    // Prepare path and launch async write task
    std::string block_path = this->block_path(block_count_++);

    return std::async(std::launch::async, [sorted_terms = std::move(sorted_terms), block_path]() {
        // Use ofstream for RAII file handling
        std::ofstream out(block_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            spdlog::error("Failed to open block file for writing: {}", block_path);
            return;
        }

        uint32_t num_terms = sorted_terms.size();
        out.write(reinterpret_cast<const char*>(&num_terms), sizeof(num_terms));

        for (const auto& [term, postings] : sorted_terms) {
            uint32_t term_len = term.size();
            out.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
            out.write(term.c_str(), term_len);

            uint32_t postings_size = postings.size();
            out.write(reinterpret_cast<const char*>(&postings_size), sizeof(postings_size));

            // Calculate and write sync points
            std::vector<SyncPoint> sync_points;
            sync_points.reserve(postings_size / PostingList::SYNC_INTERVAL + 1);
            for (uint32_t i = 0; i < postings.size(); i += PostingList::SYNC_INTERVAL) {
                SyncPoint sp{postings[i].doc_id, i};
                sync_points.push_back(sp);
            }

            uint32_t sync_points_size = sync_points.size();
            out.write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
            if (sync_points_size > 0) {
                out.write(reinterpret_cast<const char*>(sync_points.data()), sync_points_size * sizeof(SyncPoint));
            }

            // Write the actual postings
            if (postings_size > 0) {
                out.write(reinterpret_cast<const char*>(postings.data()), postings_size * sizeof(Posting));
            }
        }
        // ofstream destructor handles closing
    });
}

std::string IndexBuilder::merge_block_subset(
    const std::vector<std::string>& block_paths, size_t start_idx, size_t end_idx, int tier_num, bool is_final_output) {
    std::string output_path;
    if (is_final_output) {
        output_path = output_dir_ + "/final_index.data";
    } else {
        output_path = output_dir_ + "/blocks/intermediate_t" + std::to_string(tier_num) + "_" +
                      std::to_string(start_idx) + "_" + std::to_string(end_idx) + ".data";
    }

    data::FileWriter out(output_path.c_str());

    uint32_t total_terms = 0;
    out.Write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

    using BlockReaderPtr = std::unique_ptr<BlockReader>;
    auto cmp = [](const BlockReaderPtr& a, const BlockReaderPtr& b) {
        if (!a)
            return false;
        if (!b)
            return true;
        return a->current_term > b->current_term;
    };
    std::priority_queue<BlockReaderPtr, std::vector<BlockReaderPtr>, decltype(cmp)> pq(cmp);

    // Open only the subset of blocks
    for (size_t i = start_idx; i < end_idx && i < block_paths.size(); i++) {
        try {
            auto reader = std::make_unique<BlockReader>(block_paths[i]);
            if (reader->has_next) {
                pq.push(std::move(reader));
            } else {
                spdlog::warn("Block reader for {} reported no data upon opening.", block_paths[i]);
                std::error_code ec;
                std::filesystem::remove(block_paths[i], ec);
                if (ec)
                    spdlog::warn("Failed to remove empty block {}: {}", block_paths[i], ec.message());
            }
        } catch (const std::exception& e) {
            spdlog::error("Error opening block {}: {}", block_paths[i], e.what());
        }
    }

    std::vector<Posting> merged_postings;
    while (!pq.empty()) {
        std::string current_term = pq.top()->current_term;
        merged_postings.clear();

        // Merge all postings for the current term
        while (!pq.empty() && pq.top()->current_term == current_term) {
            BlockReaderPtr reader = std::move(const_cast<BlockReaderPtr&>(pq.top()));
            pq.pop();

            // Efficiently move or append postings
            merged_postings.insert(merged_postings.end(),
                                   std::make_move_iterator(reader->current_postings.begin()),
                                   std::make_move_iterator(reader->current_postings.end()));
            reader->current_postings.clear();
            try {
                reader->read_next();
                if (reader->has_next) {
                    pq.push(std::move(reader));
                }
            } catch (const std::exception& e) {
                spdlog::error("Error reading from block during merge for term '{}': {}", current_term, e.what());
            }
        }

        std::sort(merged_postings.begin(), merged_postings.end(), [](const Posting& a, const Posting& b) {
            return a.doc_id < b.doc_id;
        });

        // Write term string
        uint32_t term_len = current_term.size();
        out.Write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        out.Write(current_term.c_str(), term_len);

        // Write postings count
        uint32_t postings_size = merged_postings.size();
        out.Write(reinterpret_cast<const char*>(&postings_size), sizeof(postings_size));

        // Calculate and write sync points for postings
        std::vector<SyncPoint> sync_points;
        if (postings_size > 0) {
            sync_points.reserve(postings_size / PostingList::SYNC_INTERVAL + 1);
            for (uint32_t i = 0; i < postings_size; i += PostingList::SYNC_INTERVAL) {
                SyncPoint sp{merged_postings[i].doc_id, i};
                sync_points.push_back(sp);
            }
        }

        uint32_t sync_points_size = sync_points.size();
        out.Write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
        if (sync_points_size > 0) {
            out.Write(reinterpret_cast<const char*>(sync_points.data()), sync_points_size * sizeof(SyncPoint));
        }

        if (!merged_postings.empty()) {
            if (is_final_output) {
                // Use VByte encoding for doc_id deltas and frequencies (final format)
                uint32_t last_doc_id = 0;
                std::vector<uint32_t> doc_id_deltas;
                std::vector<uint32_t> freqs;
                doc_id_deltas.reserve(postings_size);
                freqs.reserve(postings_size);

                for (const auto& posting : merged_postings) {
                    doc_id_deltas.push_back(posting.doc_id - last_doc_id);
                    freqs.push_back(posting.freq);
                    last_doc_id = posting.doc_id;
                }
                VByteCodec::encodeBatch(doc_id_deltas, out);
                VByteCodec::encodeBatch(freqs, out);
            } else {
                out.Write(reinterpret_cast<const char*>(merged_postings.data()), postings_size * sizeof(Posting));
            }
        }

        total_terms++;
    }

    // Update total terms count at the beginning of the file
    out.Fseek(0);
    out.Write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));
    out.Close();

    for (size_t i = start_idx; i < end_idx && i < block_paths.size(); i++) {
        std::error_code ec;
        std::filesystem::remove(block_paths[i], ec);
        if (ec) {
            spdlog::warn("Failed to remove intermediate block {}: {}", block_paths[i], ec.message());
        }
    }

    return output_path;
}

void IndexBuilder::merge_blocks_tiered() {
    if (block_count_ <= 0) {
        spdlog::info("No blocks generated, skipping merge.");
        std::ofstream out(output_dir_ + "/final_index.data", std::ios::binary | std::ios::trunc);
        uint32_t zero = 0;
        if (out) {
            out.write(reinterpret_cast<char*>(&zero), sizeof(zero));
        }
        return;
    }

    std::vector<std::string> current_tier;
    current_tier.reserve(block_count_);
    for (int i = 0; i < block_count_; ++i) {
        current_tier.push_back(block_path(i));
    }

    int tier_number = 0;
    while (current_tier.size() > 1) {
        tier_number++;
        spdlog::info("Processing merge tier {}: merging {} blocks with factor {}",
                     tier_number,
                     current_tier.size(),
                     MERGE_FACTOR);

        std::vector<std::string> next_tier;
        const size_t num_groups = (current_tier.size() + MERGE_FACTOR - 1) / MERGE_FACTOR;
        next_tier.reserve(num_groups);

        for (size_t i = 0; i < current_tier.size(); i += MERGE_FACTOR) {
            const size_t end_idx = std::min(i + MERGE_FACTOR, current_tier.size());
            std::string merged_block = merge_block_subset(current_tier,
                                                          i,
                                                          end_idx,
                                                          tier_number,
                                                          /*is_final_output=*/false);
            next_tier.emplace_back(std::move(merged_block));
        }

        current_tier = std::move(next_tier);
        spdlog::info("Tier {} complete, produced {} blocks", tier_number, current_tier.size());
    }

    if (current_tier.size() == 1) {
        spdlog::info("Finalizing index format...");
        merge_block_subset(current_tier,
                           0,
                           1,
                           tier_number + 1,  // Final tier number
                           /*is_final_output=*/true);
    }
}

void IndexBuilder::save_document_map() {
    std::string map_path = output_dir_ + "/document_map.data";
    std::ofstream out(map_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("Failed to open document map file for writing: {}", map_path);
        return;
    }

    uint32_t num_docs = 0;
    {
        std::lock_guard<std::mutex> lock(document_mutex_);
        num_docs = document_metadata_.size();
        out.write(reinterpret_cast<const char*>(&num_docs), sizeof(num_docs));

        for (const auto& meta : document_metadata_) {
            out.write(reinterpret_cast<const char*>(&meta.id), sizeof(meta.id));

            uint32_t url_len = meta.url.size();
            out.write(reinterpret_cast<const char*>(&url_len), sizeof(url_len));
            out.write(meta.url.c_str(), url_len);

            std::string joined_title = join_title(meta.title);
            uint32_t title_len = joined_title.size();
            out.write(reinterpret_cast<const char*>(&title_len), sizeof(title_len));
            out.write(joined_title.c_str(), title_len);

            // bm25f
            out.write(reinterpret_cast<const char*>(&meta.body_length), sizeof(meta.body_length));
            out.write(reinterpret_cast<const char*>(&meta.title_length), sizeof(meta.title_length));
            out.write(reinterpret_cast<const char*>(&meta.url_length), sizeof(meta.url_length));
            out.write(reinterpret_cast<const char*>(&meta.desc_length), sizeof(meta.desc_length));
            out.write(reinterpret_cast<const char*>(&meta.pagerank_score), sizeof(meta.pagerank_score));
        }
    }
    spdlog::info("Saved document map with {} entries to {}", num_docs, map_path);
}

void IndexBuilder::save_index_stats() {
    std::string stats_path = output_dir_ + "/index_stats.data";
    std::ofstream stats_file(stats_path, std::ios::binary);

    if (!stats_file) {
        spdlog::error("Failed to create index stats file: {}", stats_path);
        return;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_file.write(reinterpret_cast<const char*>(&stats_.doc_count), sizeof(stats_.doc_count));

    stats_file.write(reinterpret_cast<const char*>(&stats_.total_body_length), sizeof(stats_.total_body_length));
    stats_file.write(reinterpret_cast<const char*>(&stats_.total_title_length), sizeof(stats_.total_title_length));
    stats_file.write(reinterpret_cast<const char*>(&stats_.total_url_length), sizeof(stats_.total_url_length));
    stats_file.write(reinterpret_cast<const char*>(&stats_.total_desc_length), sizeof(stats_.total_desc_length));

    spdlog::info("Saved index statistics to {}", stats_path);
}

void IndexBuilder::create_term_dictionary() {
    std::string index_path = output_dir_ + "/final_index.data";
    std::string dict_path = output_dir_ + "/term_dictionary.data";

    // Use RAII for mmap'ed file access
    std::unique_ptr<MappedFileReader> index_reader;
    try {
        index_reader = std::make_unique<MappedFileReader>(index_path);
    } catch (const std::exception& e) {
        return;
    }

    if (!index_reader->isValid() || index_reader->size() < sizeof(uint32_t)) {
        spdlog::error("Index file '{}' is invalid or too small (size: {}) for dictionary creation.",
                      index_path,
                      index_reader->size());
        return;
    }
    const char* data = index_reader->data();
    size_t index_size = index_reader->size();


    // Use ofstream for RAII dictionary file writing
    std::ofstream dict_stream(dict_path, std::ios::binary | std::ios::trunc);
    if (!dict_stream) {
        spdlog::error("Failed to create dictionary file: {}", dict_path);
        return;
    }

    const char* ptr = data;
    const char* end_ptr = data + index_size;

    uint32_t term_count = 0;
    if (ptr + sizeof(uint32_t) <= end_ptr) {
        term_count = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
    } else {
        spdlog::error("Index file too small to read term count.");
        return;
    }

    std::vector<std::pair<std::string, uint64_t>> term_entries;
    if (term_count > 0) {
        try {
            term_entries.reserve(term_count);
        } catch (const std::bad_alloc& e) {
            spdlog::error("Failed to reserve memory for {} term entries: {}", term_count, e.what());
            return;
        }
    }

    const char* term_data_start_ptr = ptr;
    uint64_t current_entry_start_offset = 0;

    for (uint32_t i = 0; i < term_count; i++) {
        const char* current_read_ptr = term_data_start_ptr + current_entry_start_offset;
        if (current_read_ptr + sizeof(uint32_t) > end_ptr) {
            spdlog::error("Index file ended unexpectedly while reading term length at term index {}", i);
            return;
        }
        uint32_t term_len = *reinterpret_cast<const uint32_t*>(current_read_ptr);
        current_read_ptr += sizeof(uint32_t);
        if (current_read_ptr + term_len > end_ptr) {
            spdlog::error(
                "Index file ended unexpectedly while reading term string (len {}) at term index {}", term_len, i);
            return;
        }
        std::string term(current_read_ptr, term_len);
        current_read_ptr += term_len;

        if (current_read_ptr + sizeof(uint32_t) > end_ptr) {
            spdlog::error(
                "Index file ended unexpectedly while reading postings size for term '{}' at term index {}", term, i);
            return;
        }
        uint32_t postings_size = *reinterpret_cast<const uint32_t*>(current_read_ptr);
        current_read_ptr += sizeof(uint32_t);

        if (current_read_ptr + sizeof(uint32_t) > end_ptr) {
            spdlog::error(
                "Index file ended unexpectedly while reading sync points size for term '{}' at term index {}", term, i);
            return;
        }
        uint32_t sync_points_size = *reinterpret_cast<const uint32_t*>(current_read_ptr);
        current_read_ptr += sizeof(uint32_t);

        size_t sync_points_bytes = static_cast<size_t>(sync_points_size) * sizeof(SyncPoint);
        if (current_read_ptr + sync_points_bytes > end_ptr) {
            spdlog::error("Index file ended unexpectedly while reading sync points data ({} bytes) for term '{}' at "
                          "term index {}",
                          sync_points_bytes,
                          term,
                          i);
            return;
        }
        current_read_ptr += sync_points_bytes;  // Skip sync points data

        // Skip VByte-encoded postings
        const char* postings_start_ptr = current_read_ptr;
        for (uint32_t j = 0; j < postings_size; j++) {
            // Skip doc_id delta
            while (current_read_ptr < end_ptr && (*current_read_ptr & 0x80))
                current_read_ptr++;
            if (current_read_ptr >= end_ptr) {
                spdlog::error("Index file ended unexpectedly skipping docID delta (term {}, posting {})", i, j);
                return;
            }
            current_read_ptr++;

            // Skip frequency
            while (current_read_ptr < end_ptr && (*current_read_ptr & 0x80))
                current_read_ptr++;
            if (current_read_ptr >= end_ptr) {
                spdlog::error("Index file ended unexpectedly skipping freq (term {}, posting {})", i, j);
                return;
            }
            current_read_ptr++;                // Skip final byte of freq
            if (current_read_ptr > end_ptr) {  // Should be caught by checks above, but good safety net
                spdlog::error("Index file pointer went past end after skipping postings (term {}, posting {})", i, j);
                return;
            }
        }
        // const char* postings_end_ptr = current_read_ptr; // Mark end if needed

        // Store term with its offset relative to the start of the term data section
        term_entries.emplace_back(term, current_entry_start_offset);

        // Update the offset for the *start* of the next term's entry
        current_entry_start_offset = current_read_ptr - term_data_start_ptr;
        if (i > 0 && (i % 500000 == 0 || i == term_count - 1)) {
            spdlog::info("Collected {}/{} terms for dictionary...", i + 1, term_count);
        }
    }

    if (term_data_start_ptr + current_entry_start_offset > end_ptr) {
        spdlog::error("Error: Calculated end offset ({}) went past end of mapped data ({}) after processing terms.",
                      current_entry_start_offset,
                      index_size - (term_data_start_ptr - data));
        return;
    }

    spdlog::info("Sorting {} term entries...", term_entries.size());
    std::sort(term_entries.begin(), term_entries.end());

    // Write dict heade
    uint32_t magic = 0x4D495448;  // "MITH"
    uint32_t version = 1;
    dict_stream.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    dict_stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
    // Write the actual number of entries we sorted (might differ from header if file was truncated)
    uint32_t actual_term_count = term_entries.size();
    dict_stream.write(reinterpret_cast<const char*>(&actual_term_count), sizeof(actual_term_count));

    spdlog::info("Writing dictionary with {} entries...", actual_term_count);

    // Write dictionary entries (simple format: len, term, offset, postings_count)
    uint64_t last_logged_count = 0;
    for (uint32_t i = 0; i < actual_term_count; ++i) {
        const auto& [term, offset] = term_entries[i];

        // Read postings count directly from the mapped index data using the stored offset
        uint32_t postings_count = 0;
        const char* entry_ptr = term_data_start_ptr + offset;
        // Bounds check before reading postings count
        // Need offset + term_len_bytes + term_bytes to get to postings count field
        const char* count_ptr = entry_ptr + sizeof(uint32_t) + term.length();
        if (count_ptr + sizeof(uint32_t) <= end_ptr) {
            postings_count = *reinterpret_cast<const uint32_t*>(count_ptr);
        } else {
            spdlog::warn(
                "Could not read postings count for term '{}' at offset {}: index bounds exceeded.", term, offset);
        }


        // Write term length and data
        uint32_t term_len = term.length();
        dict_stream.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        dict_stream.write(term.data(), term_len);

        // Write offset (absolute offset within the term data section of final_index.data)
        dict_stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        // Write postings count
        dict_stream.write(reinterpret_cast<const char*>(&postings_count), sizeof(postings_count));
        if (i > 0 && (i % 500000 == 0 || i == actual_term_count - 1)) {
            spdlog::info("Wrote {}/{} dictionary entries...", i + 1, actual_term_count);
        }
    }

    spdlog::info("Term dictionary creation complete: {}", dict_path);
}


std::string IndexBuilder::block_path(int block_num) const {
    return output_dir_ + "/blocks/block_" + std::to_string(block_num) + ".data";
}

void IndexBuilder::finalize() {
    spdlog::info("Finalizing index build...");
    // Wait for doc processing tasks to complete
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        spdlog::info("Waiting for {} active tasks and task queue to empty...", active_tasks_.load());
        condition_.wait(lock, [this] { return tasks_.empty() && active_tasks_.load() == 0; });
    }
    spdlog::info("All document processing tasks completed.");

    // Flush any remaining terms in the last block
    if (current_block_term_count_ > 0) {
        spdlog::info("Flushing final block ({} unique terms)...", current_block_term_count_);
        auto flush_future = flush_block();
        if (flush_future.valid()) {
            try {
                flush_future.wait();
                spdlog::info("Final block flush completed.");
            } catch (const std::exception& e) {
                spdlog::error("Exception waiting for final block flush: {}", e.what());
            }
        }
    } else {
        spdlog::info("No final block to flush (term count was 0).");
    }

    spdlog::info("Starting block merge process with {} blocks...", block_count_);
    merge_blocks_tiered();  // Handles 0, 1, or N blocks

    spdlog::info("Saving document map (approx {} documents)...", document_metadata_.size());
    save_document_map();  // Handles its own locking now

    spdlog::info("Saving index statistics for static ranking...");
    save_index_stats();
    // quick_stats_check(output_dir_ + "/index_stats.data");

    spdlog::info("Finalizing position index...");
    PositionIndex::finalizeIndex(output_dir_);

    // Only create dictionary if a final index was actually created
    std::string final_index_path = output_dir_ + "/final_index.data";
    std::error_code fs_ec;
    bool index_exists = std::filesystem::exists(final_index_path, fs_ec);
    uintmax_t index_size = 0;
    if (index_exists && !fs_ec) {
        index_size = std::filesystem::file_size(final_index_path, fs_ec);
    }

    if (index_exists && !fs_ec && index_size > sizeof(uint32_t)) {
        spdlog::info("Creating term dictionary from final index...");
        create_term_dictionary();
    } else {
        if (!index_exists) {
            spdlog::warn("Skipping term dictionary creation: Final index file {} not found.", final_index_path);
        } else if (fs_ec) {
            spdlog::warn("Skipping term dictionary creation: Error checking final index file {}: {}",
                         final_index_path,
                         fs_ec.message());
        } else {  // Exists but size is too small
            spdlog::warn("Skipping term dictionary creation: Final index file {} is too small ({} bytes).",
                         final_index_path,
                         index_size);
        }
    }


    spdlog::info("Cleaning up temporary block files...");
    std::error_code rm_ec;
    std::filesystem::remove_all(output_dir_ + "/blocks", rm_ec);
    if (rm_ec) {
        spdlog::error("Failed to remove temporary block directory '{}': {}", output_dir_ + "/blocks", rm_ec.message());
    }

    spdlog::info("Index finalization complete.");
}

}  // namespace mithril
