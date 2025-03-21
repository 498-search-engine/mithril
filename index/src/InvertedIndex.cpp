#include "InvertedIndex.h"

#include "PositionIndex.h"
#include "TextPreprocessor.h"
#include "Utils.h"
#include "data/Deserialize.h"
#include "data/Gzip.h"
#include "data/Reader.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril {

IndexBuilder::IndexBuilder(const std::string& output_dir, size_t num_threads) : output_dir_(output_dir) {
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
        worker.join();
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
        task();
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            active_tasks_--;
        }
        condition_.notify_all();
    }
}

bool IndexBuilder::parse_link_line(const std::string& line, std::string& url, std::string& title) {
    size_t paren_start = line.find('(');
    size_t paren_end = line.find(')');
    if (paren_start == std::string::npos || paren_end == std::string::npos) {
        return false;
    }

    url = line.substr(0, paren_start - 1);
    title = line.substr(paren_start + 2, paren_end - paren_start - 3);
    return true;
}

std::vector<std::string> IndexBuilder::read_words(const std::string& path) {
    std::vector<std::string> words;
    std::ifstream file(path);
    std::string word;
    while (std::getline(file, word)) {
        std::string normalized = TokenNormalizer::normalize(word);
        if (!normalized.empty()) {
            words.push_back(normalized);
        }
    }
    return words;
}

size_t IndexBuilder::estimate_memory_usage(const Document& doc) {
    static constexpr size_t AVG_BYTES_PER_WORD = 20;  // Includes all overhead
    size_t total_words = doc.title.size() + doc.words.size();
    return total_words * AVG_BYTES_PER_WORD;
}

bool IndexBuilder::should_flush(const Document& doc) {
    return (current_block_size_ + estimate_memory_usage(doc)) >= MAX_BLOCK_SIZE;
}

void IndexBuilder::add_terms(data::docid_t doc_id, const std::unordered_map<std::string, uint32_t>& term_freqs) {
    for (const auto& [term, freq] : term_freqs) {
        auto& postings = dictionary_.get_or_create(term);
        postings.add({doc_id, freq});
        current_block_size_ += sizeof(Posting) + term.size();
    }
}

std::string IndexBuilder::StringVecToString(const std::vector<std::string>& vec) {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result += vec[i];
        if (i < vec.size() - 1) {
            result += " ";
        }
    }
    return result;
}

void IndexBuilder::process_document(const Document& doc) {
    // Check if we need to flush the current block
    if (should_flush(doc)) {
        auto future = flush_block();
        future.wait();
    }

    auto task = [this, doc]() {
        const size_t estimated_unique_terms = doc.words.size() / 4;  // ~25% unique term ratio
        std::unordered_map<std::string, uint32_t> term_freqs;
        term_freqs.reserve(estimated_unique_terms);
        std::unordered_map<std::string, std::vector<uint32_t>> term_positions;
        term_positions.reserve(estimated_unique_terms);

        // Global position counter across all fields
        uint32_t position = 0;
        size_t total_term_count = 0;  // For calculating frequency ratios

        // Process URL - tokenize and normalize
        std::string url_copy = doc.url;
        // Replace common delimiters with spaces for tokenization
        std::replace(url_copy.begin(), url_copy.end(), '/', ' ');
        std::replace(url_copy.begin(), url_copy.end(), '.', ' ');
        std::replace(url_copy.begin(), url_copy.end(), '-', ' ');
        std::replace(url_copy.begin(), url_copy.end(), '_', ' ');
        std::replace(url_copy.begin(), url_copy.end(), '?', ' ');
        std::replace(url_copy.begin(), url_copy.end(), '&', ' ');
        std::replace(url_copy.begin(), url_copy.end(), '=', ' ');

        std::istringstream url_stream(url_copy);
        std::string url_part;
        while (url_stream >> url_part) {
            std::string normalized = TokenNormalizer::normalize(url_part, FieldType::URL);
            if (!normalized.empty()) {
                term_freqs[normalized]++;
                term_positions[normalized].push_back(position++);
                total_term_count++;
            }
        }

        // Process title words with TITLE field type
        for (const auto& word : doc.title) {
            std::string normalized = TokenNormalizer::normalize(word, FieldType::TITLE);
            if (!normalized.empty()) {
                term_freqs[normalized]++;
                term_positions[normalized].push_back(position++);
                total_term_count++;
            }
        }

        // Process description words with DESC field type
        for (const auto& word : doc.description) {
            std::string normalized = TokenNormalizer::normalize(word, FieldType::DESC);
            if (!normalized.empty()) {
                term_freqs[normalized]++;
                term_positions[normalized].push_back(position++);
            }
        }

        // Process body words with default BODY field type
        for (const auto& word : doc.words) {
            std::string normalized = TokenNormalizer::normalize(word, FieldType::BODY);
            if (!normalized.empty()) {
                term_freqs[normalized]++;
                term_positions[normalized].push_back(position++);
                total_term_count++;
            }
        }

        // Assign into document map
        {
            std::unique_lock<std::mutex> lock(document_mutex_);
            url_to_id_[doc.url] = doc.id;
            documents_.push_back(doc);
        }

        // Prepare batch for position index with selective indexing
        std::vector<std::pair<std::string, std::vector<uint32_t>>> position_batch;
        position_batch.reserve(term_positions.size() / 2);  // maybe ~50% of terms will qualify

        for (const auto& [term, freq] : term_freqs) {
            if (PositionIndex::shouldStorePositions(term, freq, total_term_count)) {
                auto it = term_positions.find(term);
                if (it != term_positions.end()) {
                    position_batch.emplace_back(term, std::move(it->second));
                }
            }
        }

        // Write positions to position index in a single batch
        if (!position_batch.empty()) {
            PositionIndex::addPositionsBatch(output_dir_, doc.id, position_batch);
        }

        // Add terms to the main inverted index with freqs
        {
            std::lock_guard<std::mutex> lock(block_mutex_);
            for (const auto& [term, freq] : term_freqs) {
                auto& postings = dictionary_.get_or_create(term);
                postings.add(doc.id, freq);
                current_block_size_ += sizeof(Posting);
            }
        }
    };

    // Queue the task
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(task);
    }
    condition_.notify_one();
}

std::string IndexBuilder::join_title(const std::vector<std::string>& title_words) {
    std::string result;
    for (size_t i = 0; i < title_words.size(); ++i) {
        result += title_words[i];
        if (i < title_words.size() - 1) {
            result += " ";
        }
    }
    return result;
}

void IndexBuilder::add_document(const std::string& doc_path) {
    Document doc;
    {
        data::FileReader file{doc_path.c_str()};
        data::GzipReader gzip{file};
        if (!data::DeserializeValue(doc, gzip)) {
            throw std::runtime_error("Failed to deserialize document: " + doc_path);
        }
    }

    process_document(doc);
}

std::future<void> IndexBuilder::flush_block() {
    if (current_block_size_ == 0) {
        return std::async(std::launch::deferred, []() {});
    }

    // Get sorted terms and their postings
    std::vector<std::tuple<std::string, std::vector<Posting>>> sorted_terms;
    dictionary_.iterate_terms([&](const std::string& term, const PostingList& postings) {
        if (!postings.empty())
            sorted_terms.emplace_back(term, postings.postings());
    });

    if (sorted_terms.empty()) {
        return std::async(std::launch::deferred, []() {});
    }

    // Reset block state
    current_block_size_ = 0;
    dictionary_.clear_postings();

    // Write block asynchronously
    std::string block_path = this->block_path(block_count_++);
    return std::async(std::launch::async, [sorted_terms = std::move(sorted_terms), block_path]() {
        std::ofstream out(block_path, std::ios::binary);
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
            for (uint32_t i = 0; i < postings.size(); i += PostingList::SYNC_INTERVAL) {
                if (i < postings.size()) {
                    SyncPoint sp{postings[i].doc_id, i};
                    sync_points.push_back(sp);
                }
            }

            uint32_t sync_points_size = sync_points.size();
            out.write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
            if (sync_points_size > 0) {
                out.write(reinterpret_cast<const char*>(sync_points.data()), sync_points_size * sizeof(SyncPoint));
            }

            // Write the actual postings
            out.write(reinterpret_cast<const char*>(postings.data()), postings_size * sizeof(Posting));
        }
    });
}

std::string IndexBuilder::merge_block_subset(const std::vector<std::string>& block_paths,
                                             size_t start_idx,
                                             size_t end_idx,
                                             bool is_final_output) {
    std::string output_path;
    if (is_final_output) {
        output_path = output_dir_ + "/final_index.data";
    } else {
        output_path =
            output_dir_ + "/blocks/intermediate_" + std::to_string(start_idx) + "_" + std::to_string(end_idx) + ".data";
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to create output file: " + output_path);
    }

    uint32_t total_terms = 0;
    out.write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

    using BlockReaderPtr = std::unique_ptr<BlockReader>;
    auto cmp = [](const BlockReaderPtr& a, const BlockReaderPtr& b) { return a->current_term > b->current_term; };
    std::priority_queue<BlockReaderPtr, std::vector<BlockReaderPtr>, decltype(cmp)> pq(cmp);

    // Open only the subset of blocks
    for (size_t i = start_idx; i < end_idx && i < block_paths.size(); i++) {
        try {
            auto reader = std::make_unique<BlockReader>(block_paths[i]);
            if (reader->has_next) {
                pq.push(std::move(reader));
            }
        } catch (const std::exception& e) {
            std::cerr << "Error opening block " << block_paths[i] << ": " << e.what() << "\n";
        }
    }

    // Merge blocks using the same logic as merge_blocks
    while (!pq.empty()) {
        std::string current_term = pq.top()->current_term;
        std::vector<Posting> merged_postings;

        // Merge all postings for the current term
        while (!pq.empty() && pq.top()->current_term == current_term) {
            BlockReaderPtr reader = std::move(const_cast<BlockReaderPtr&>(pq.top()));
            pq.pop();

            // Simply add the postings
            for (auto& posting : reader->current_postings) {
                merged_postings.push_back(posting);
            }

            try {
                reader->read_next();
                if (reader->has_next) {
                    pq.push(std::move(reader));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error reading block: " << e.what() << "\n";
            }
        }

        // Sort merged postings by doc_id
        std::sort(merged_postings.begin(), merged_postings.end(), [](const Posting& a, const Posting& b) {
            return a.doc_id < b.doc_id;
        });

        // Write term string
        uint32_t term_len = current_term.size();
        out.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        out.write(current_term.c_str(), term_len);

        // Write postings count
        uint32_t postings_size = merged_postings.size();
        out.write(reinterpret_cast<const char*>(&postings_size), sizeof(postings_size));

        // Calculate and write sync points for postings
        std::vector<SyncPoint> sync_points;
        for (uint32_t i = 0; i < postings_size; i += PostingList::SYNC_INTERVAL) {
            if (i < postings_size) {
                SyncPoint sp{merged_postings[i].doc_id, i};
                sync_points.push_back(sp);
            }
        }

        uint32_t sync_points_size = sync_points.size();
        out.write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
        if (sync_points_size > 0) {
            out.write(reinterpret_cast<const char*>(sync_points.data()), sync_points_size * sizeof(SyncPoint));
        }

        if (is_final_output) {
            // Use VByte encoding for doc_id deltas and frequencies (final format)
            uint32_t last_doc_id = 0;
            std::vector<uint32_t> doc_id_deltas;
            std::vector<uint32_t> freqs;

            for (const auto& posting : merged_postings) {
                doc_id_deltas.push_back(posting.doc_id - last_doc_id);
                freqs.push_back(posting.freq);
                last_doc_id = posting.doc_id;
            }
            VByteCodec::encodeBatch(doc_id_deltas, out);
            VByteCodec::encodeBatch(freqs, out);
        } else {
            // Use raw Posting structs for intermediate blocks
            out.write(reinterpret_cast<const char*>(merged_postings.data()), postings_size * sizeof(Posting));
        }

        total_terms++;
    }

    // Update total terms count at the beginning of the file
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

    // Remove the merged blocks to save space
    for (size_t i = start_idx; i < end_idx && i < block_paths.size(); i++) {
        std::filesystem::remove(block_paths[i]);
    }

    return output_path;
}

void IndexBuilder::merge_blocks_tiered() {
    if (block_count_ <= 1) {
        if (block_count_ == 1) {
            spdlog::info("Single block detected, processing to create final index");
            std::vector<std::string> single_block = {block_path(0)};
            merge_block_subset(single_block, 0, 1, true);
        }
        return;
    }

    std::vector<std::string> current_tier;
    for (int i = 0; i < block_count_; ++i) {
        current_tier.push_back(block_path(i));
    }

    int tier_number = 0;
    while (current_tier.size() > 1) {
        tier_number++;
        spdlog::info(
            "Processing tier {}: merging {} blocks with factor {}", tier_number, current_tier.size(), MERGE_FACTOR);

        std::vector<std::string> next_tier;
        for (size_t i = 0; i < current_tier.size(); i += MERGE_FACTOR) {
            size_t end_idx = std::min(i + MERGE_FACTOR, current_tier.size());
            spdlog::debug("Merging blocks {}-{} of {}", i, end_idx - 1, current_tier.size());
            std::string merged_block = merge_block_subset(current_tier, i, end_idx, false);
            next_tier.push_back(merged_block);
        }
        current_tier = std::move(next_tier);
        spdlog::info("Tier {} complete, produced {} blocks", tier_number, current_tier.size());
    }

    if (current_tier.size() == 1) {
        spdlog::info("Creating final index from last block");
        merge_block_subset(current_tier, 0, 1, true);
    }
}

void IndexBuilder::save_document_map() {
    std::ofstream out(output_dir_ + "/document_map.data", std::ios::binary);

    uint32_t num_docs = documents_.size();
    out.write(reinterpret_cast<const char*>(&num_docs), sizeof(num_docs));

    for (const auto& doc : documents_) {
        uint32_t url_len = doc.url.size();
        out.write(reinterpret_cast<const char*>(&doc.id), sizeof(doc.id));
        out.write(reinterpret_cast<const char*>(&url_len), sizeof(url_len));
        out.write(doc.url.c_str(), url_len);

        std::string joined_title;
        for (size_t i = 0; i < doc.title.size(); ++i) {
            joined_title += doc.title[i];
            if (i < doc.title.size() - 1)
                joined_title += " ";
        }

        uint32_t title_len = joined_title.size();
        out.write(reinterpret_cast<const char*>(&title_len), sizeof(title_len));
        out.write(joined_title.c_str(), title_len);
    }
}

void IndexBuilder::create_term_dictionary() {
    std::string index_path = output_dir_ + "/final_index.data";
    std::string dict_path = output_dir_ + "/term_dictionary.data";

    int fd = open(index_path.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open index file for dictionary creation" << std::endl;
        return;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "Failed to get index file size" << std::endl;
        close(fd);
        return;
    }

    const char* data = static_cast<const char*>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        std::cerr << "Failed to memory map index file" << std::endl;
        close(fd);
        return;
    }

    FILE* dict_file = fopen(dict_path.c_str(), "wb");
    if (!dict_file) {
        std::cerr << "Failed to create dictionary file" << std::endl;
        munmap(const_cast<char*>(data), sb.st_size);
        close(fd);
        return;
    }

    std::vector<char> write_buffer(16 * 1024 * 1024);
    setvbuf(dict_file, write_buffer.data(), _IOFBF, write_buffer.size());

    const char* ptr = data;
    uint32_t term_count = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);

    std::cout << "Creating dictionary for " << term_count << " terms" << std::endl;

    std::vector<std::pair<std::string, uint64_t>> term_entries;
    term_entries.reserve(term_count);

    const char* term_start_ptr = ptr;
    for (uint32_t i = 0; i < term_count; i++) {
        // Calculate relative offset from start of terms section
        uint64_t term_offset = ptr - term_start_ptr;

        // Read term length and term
        uint32_t term_len = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(term_len);

        // Create actual string (needed for sorting and compression)
        std::string term(ptr, term_len);
        ptr += term_len;

        // Read postings info (needed for skipping to next term)
        uint32_t postings_size = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);

        // Skip sync points size and data
        uint32_t sync_points_size = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        ptr += sync_points_size * sizeof(SyncPoint);

        // Skip VByte-encoded postings
        for (uint32_t j = 0; j < postings_size; j++) {
            // Skip doc_id delta
            while (*ptr & 0x80)
                ptr++;
            ptr++;

            // Skip frequency
            while (*ptr & 0x80)
                ptr++;
            ptr++;
        }

        // Store term with offset
        term_entries.emplace_back(term, term_offset);

        if (i % 100000 == 0 || i == term_count - 1) {
            std::cout << "\rCollecting terms: " << i + 1 << "/" << term_count << " (" << (i + 1) * 100 / term_count
                      << "%)" << std::flush;
        }
    }

    std::cout << "\nSorting terms..." << std::endl;
    std::sort(term_entries.begin(), term_entries.end());

    // Write dict header
    uint32_t magic = 0x4D495448;  // "MITH"
    uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, dict_file);
    fwrite(&version, sizeof(version), 1, dict_file);
    fwrite(&term_count, sizeof(term_count), 1, dict_file);

    // Write compressed dict entries using front coding
    std::string prev_term;
    uint64_t prev_offset = 0;

    for (uint32_t i = 0; i < term_entries.size(); i++) {
        const auto& [term, offset] = term_entries[i];

        // Calculate delta offset (better compression)
        uint64_t delta_offset = offset - prev_offset;
        prev_offset = offset;

        // Calculate postings count (needed for query planning)
        uint32_t postings_count = 0;
        if (i < term_count) {
            // Read postings size directly from mapped index
            const char* pos_ptr = term_start_ptr + offset + sizeof(uint32_t) + term.length();
            postings_count = *reinterpret_cast<const uint32_t*>(pos_ptr);
        }

        // Write term length and data directly
        uint32_t term_len = term.length();
        fwrite(&term_len, sizeof(term_len), 1, dict_file);
        fwrite(term.data(), 1, term_len, dict_file);

        // Write offset and postings count
        fwrite(&offset, sizeof(offset), 1, dict_file);
        fwrite(&postings_count, sizeof(postings_count), 1, dict_file);

        // Show progress
        if (i % 100000 == 0 || i == term_entries.size() - 1) {
            std::cout << "\rWriting dictionary: " << i + 1 << "/" << term_entries.size() << " ("
                      << (i + 1) * 100 / term_entries.size() << "%)" << std::flush;
        }
    }

    // Clean up resources
    fclose(dict_file);
    munmap(const_cast<char*>(data), sb.st_size);
    close(fd);

    std::cout << "\nTerm dictionary creation complete" << std::endl;
}

std::string IndexBuilder::block_path(int block_num) const {
    return output_dir_ + "/blocks/block_" + std::to_string(block_num) + ".data";
}

void IndexBuilder::finalize() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        condition_.wait(lock, [this] { return tasks_.empty() && active_tasks_ == 0; });
    }
    spdlog::info("All document processing tasks completed");

    if (current_block_size_ > 0) {
        spdlog::info("Flushing final block...");
        auto flush_future = flush_block();
        flush_future.wait();
    }

    spdlog::info("Starting block merge with {} blocks...", block_count_);
    merge_blocks_tiered();

    spdlog::info("Saving document map ({} documents)...", documents_.size());
    save_document_map();

    spdlog::info("Finalizing position index...");
    PositionIndex::finalizeIndex(output_dir_);

    spdlog::info("Creating term dictionary...");
    create_term_dictionary();

    spdlog::info("Cleaning up temporary files...");
    std::filesystem::remove_all(output_dir_ + "/blocks");
    spdlog::info("Index finalization complete");
}

}  // namespace mithril
