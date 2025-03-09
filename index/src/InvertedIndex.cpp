#include "InvertedIndex.h"

#include "TextPreprocessor.h"
#include "data/Deserialize.h"
#include "data/Gzip.h"
#include "data/Reader.h"

#include <filesystem>
#include <fstream>
#include <iostream>

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
        std::unordered_map<std::string, std::vector<uint32_t>> term_positions;
        uint32_t position = 0;
        // Process title with position tracking
        for (const auto& word : doc.title) {
            std::string normalized = TokenNormalizer::normalize(word);
            if (!normalized.empty()) {
                term_positions[normalized].push_back(position++);
            }
        }
        // Process body with continued position counting
        for (const auto& word : doc.words) {
            std::string normalized = TokenNormalizer::normalize(word);
            if (!normalized.empty()) {
                term_positions[normalized].push_back(position++);
            }
        }
        // Assign into document map
        {
            std::unique_lock<std::mutex> lock(document_mutex_);
            url_to_id_[doc.url] = doc.id;
            documents_.push_back(doc);
        }
        // Add terms to the dictionary with positions
        {
            std::lock_guard<std::mutex> lock(block_mutex_);
            for (const auto& [term, positions] : term_positions) {
                auto& postings = dictionary_.get_or_create(term);
                postings.add_with_positions(doc.id, positions.size(), positions);
                current_block_size_ += sizeof(Posting) + positions.size() * sizeof(uint32_t);
            }
        }
    };

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

// remnant
// void IndexBuilder::add_document(const std::string& words_path, const std::string& links_path) {
//     auto task = [this, words_path, links_path]() {
//         // Read first line from links file.
//         std::ifstream links_file(links_path);
//         std::string line;
//         if (!std::getline(links_file, line)) {
//             std::cerr << "Empty links file: " << links_path << std::endl;
//             return;
//         }

//         std::string url, title;
//         if (!parse_link_line(line, url, title)) {
//             std::cerr << "Invalid link format: " << line << std::endl;
//             return;
//         }

//         // Assign document ID (thread-safe)
//         uint32_t doc_id;
//         {
//             std::unique_lock<std::mutex> lock(document_mutex_);
//             auto it = url_to_id_.find(url);
//             if (it == url_to_id_.end()) {
//                 doc_id = documents_.size();
//                 url_to_id_[url] = doc_id;
//                 documents_.push_back({doc_id, url, title});
//             } else {
//                 doc_id = it->second;
//             }
//         }

//         // Build a combined frequency map from the title and the words file.
//         std::unordered_map<std::string, uint32_t> term_freqs;
//         {
//             // Process title from the links file.
//             std::istringstream iss(title);
//             std::string word;
//             while (iss >> word) {
//                 std::string normalized = TokenNormalizer::normalize(word);
//                 if (!normalized.empty()) {
//                     term_freqs[normalized]++;
//                 }
//             }
//         }
//         {
//             // Process additional words from the words file.
//             auto extra_words = read_words(words_path);
//             for (const auto& w : extra_words) {
//                 term_freqs[w]++;
//             }
//         }

//         // Add terms to the current block (thread-safe)
//         {
//             std::unique_lock<std::mutex> lock(block_mutex_);
//             for (const auto& [term, freq] : term_freqs) {
//                 // current_block_[term].push_back({doc_id, freq});
//                 auto& postings = dictionary_.get_or_create(term);
//                 postings.add({doc_id, freq});
//                 current_block_size_ += sizeof(Posting) + term.size();
//             }

//             // Check if block is full
//             if (current_block_size_ >= MAX_BLOCK_SIZE) {
//                 flush_block();
//             }
//         }
//     };

//     {
//         std::unique_lock<std::mutex> lock(queue_mutex_);
//         tasks_.emplace(task);
//     }
//     condition_.notify_one();
// }

std::future<void> IndexBuilder::flush_block() {
    if (current_block_size_ == 0) {
        return std::async(std::launch::deferred, []() {});
    }

    // Get sorted terms and their postings
    std::vector<std::tuple<std::string, std::vector<Posting>, std::vector<uint32_t>>> sorted_terms;
    dictionary_.iterate_terms([&](const std::string& term, const PostingList& postings) {
        if (!postings.empty())
            sorted_terms.emplace_back(term, postings.postings(), postings.positions_store_.all_positions);
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

        for (const auto& [term, postings, positions] : sorted_terms) {
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

            // Write positions count and data 
            uint32_t positions_size = positions.size();
            out.write(reinterpret_cast<const char*>(&positions_size), sizeof(positions_size));
            if (positions_size > 0) {
                // For sync points, we need to store both position offsets and absolute positions
                std::vector<PositionSyncPoint> sync_points;
                uint32_t prev_pos = 0;
                // Compute and write sync points
                for (size_t i = 0; i < positions.size(); i++) {
                    if (i % PostingList::POSITION_SYNC_INTERVAL == 0) {
                        sync_points.push_back({static_cast<uint32_t>(i), positions[i]});
                    }
                }
                // Write sync points count and data
                uint32_t sync_points_size = sync_points.size();
                out.write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
                if (sync_points_size > 0) {
                    out.write(reinterpret_cast<const char*>(sync_points.data()), 
                              sync_points_size * sizeof(PositionSyncPoint));
                }
                // Write VByte encoded positions
                for (size_t i = 0; i < positions.size(); i++) {
                    VByteCodec::encode(positions[i], out);
                }
            }
        }
    });
}

void IndexBuilder::merge_blocks() {
    std::vector<std::string> block_paths;
    for (int i = 0; i < block_count_; ++i) {
        block_paths.push_back(block_path(i));
    }

    // Merge all blocks into the final index
    std::string final_index_path = output_dir_ + "/final_index.bin";
    std::ofstream final_out(final_index_path, std::ios::binary);

    uint32_t total_terms = 0;
    final_out.write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

    using BlockReaderPtr = std::unique_ptr<BlockReader>;
    auto cmp = [](const BlockReaderPtr& a, const BlockReaderPtr& b) { return a->current_term > b->current_term; };

    std::priority_queue<BlockReaderPtr, std::vector<BlockReaderPtr>, decltype(cmp)> pq(cmp);

    for (const auto& path : block_paths) {
        try {
            auto reader = std::make_unique<BlockReader>(path);
            if (reader->has_next) {
                pq.push(std::move(reader));
            }
        } catch (const std::exception& e) {
            std::cerr << "Error opening block " << path << ": " << e.what() << "\n";
        }
    }

    while (!pq.empty()) {
        std::string current_term = pq.top()->current_term;
        std::vector<Posting> merged_postings;
        PositionsStore merged_positions;

        // Merge all postings for current term
        while (!pq.empty() && pq.top()->current_term == current_term) {
            BlockReaderPtr reader = std::move(const_cast<BlockReaderPtr&>(pq.top()));
            pq.pop();

            // Adjust the positions offsets for the merged postings using the current merged positions count
            size_t positions_offset_adjustment = merged_positions.all_positions.size();
            for (auto& posting : reader->current_postings) {
                Posting adjusted_posting = posting;
                // UINT32_MAX indicates no positions stored.
                if (posting.positions_offset != UINT32_MAX) {
                    adjusted_posting.positions_offset += static_cast<uint32_t>(positions_offset_adjustment);
                }
                merged_postings.push_back(adjusted_posting);
            }

            // Append positions from the current block
            merged_positions.all_positions.insert(
                merged_positions.all_positions.end(),
                reader->current_positions.all_positions.begin(),
                reader->current_positions.all_positions.end()
            );

            // Read the next term from the block reader.
            try {
                reader->read_next();
                if (reader->has_next) {
                    pq.push(std::move(reader));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error reading block: " << e.what() << "\n";
            }
        }

        std::sort(merged_postings.begin(), merged_postings.end(), [](const Posting& a, const Posting& b) {
            return a.doc_id < b.doc_id;
        });

        // Write term
        uint32_t term_len = current_term.size();
        final_out.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        final_out.write(current_term.c_str(), term_len);
        
        // Write postings count and sync points
        uint32_t postings_size = merged_postings.size();
        final_out.write(reinterpret_cast<const char*>(&postings_size), sizeof(postings_size));

        // Calculate sync points (based on postings and defined sync interval)
        std::vector<SyncPoint> sync_points;
        for (uint32_t i = 0; i < postings_size; i += PostingList::SYNC_INTERVAL) {
            // i is valid because we use postings_size as upper bound.
            SyncPoint sp{merged_postings[i].doc_id, i};
            sync_points.push_back(sp);
        }
        uint32_t sync_points_size = sync_points.size();
        final_out.write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
        if (sync_points_size > 0) {
            final_out.write(reinterpret_cast<const char*>(sync_points.data()), sync_points_size * sizeof(SyncPoint));
        }

        // Write compressed postings using VByte encoding for doc_id deltas and frequency
        uint32_t last_doc_id = 0;
        for (const auto& posting : merged_postings) {
            VByteCodec::encode(posting.doc_id - last_doc_id, final_out);
            VByteCodec::encode(posting.freq, final_out);
            last_doc_id = posting.doc_id;
        }

        // For positions, write count, sync points, and VByte encoded positions
        uint32_t positions_size = merged_positions.all_positions.size();
        final_out.write(reinterpret_cast<const char*>(&positions_size), sizeof(positions_size));
        
        if (positions_size > 0) {
            // Calculate and write position sync points
            std::vector<PositionSyncPoint> sync_points;
            uint32_t absolute_pos = 0;
            
            for (size_t i = 0; i < merged_positions.all_positions.size(); i++) {
                // Update absolute position with each delta
                absolute_pos += merged_positions.all_positions[i];
                // Create sync point at regular intervals
                if (i % PostingList::POSITION_SYNC_INTERVAL == 0) {
                    sync_points.push_back({static_cast<uint32_t>(i), absolute_pos});
                }
            }
            
            uint32_t sync_points_size = sync_points.size();
            final_out.write(reinterpret_cast<const char*>(&sync_points_size), sizeof(sync_points_size));
            if (sync_points_size > 0) {
                final_out.write(reinterpret_cast<const char*>(sync_points.data()), sync_points_size * sizeof(PositionSyncPoint));
            }
            
            // Write each delta-encoded position using VByte
            for (uint32_t delta : merged_positions.all_positions) {
                VByteCodec::encode(delta, final_out);
            }
        }
        
        total_terms++;
    }

    // Update total terms count at the beginning of the file.
    final_out.seekp(0);
    final_out.write(reinterpret_cast<const char*>(&total_terms), sizeof(total_terms));

    for (const auto& path : block_paths) {
        std::filesystem::remove(path);
    }
}

void IndexBuilder::save_document_map() {
    std::ofstream out(output_dir_ + "/document_map.bin", std::ios::binary);

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

std::string IndexBuilder::block_path(int block_num) const {
    return output_dir_ + "/blocks/block_" + std::to_string(block_num) + ".bin";
}

void IndexBuilder::finalize() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        condition_.wait(lock, [this] { return tasks_.empty() && active_tasks_ == 0; });
    }

    // Force a final flush if we have any postings
    if (current_block_size_ > 0) {
        auto flush_future = flush_block();
        flush_future.wait();
    }

    merge_blocks();
    save_document_map();
    std::filesystem::remove_all(output_dir_ + "/blocks");
}
}  // namespace mithril
