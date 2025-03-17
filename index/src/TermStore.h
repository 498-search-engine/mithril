#ifndef INDEX_TERMSTORE_H
#define INDEX_TERMSTORE_H

#include "PostingBlock.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mithril {

class PostingList {
public:
    void add(const Posting& posting);
    void add(uint32_t doc_id, uint32_t freq);
    void add_with_positions(uint32_t doc_id, uint32_t freq, const std::vector<uint32_t>& positions);
    std::vector<uint32_t> get_positions(size_t posting_index) const;
    const std::vector<PositionSyncPoint>& position_sync_points() const { return position_sync_points_; }
    const std::vector<Posting>& postings() const;
    size_t find_nearest_position_sync_point(uint32_t target_position) const;
    size_t size_bytes() const;
    void clear();
    bool empty() const;
    static constexpr uint32_t SYNC_INTERVAL = 1024 * 1024;               // 1 MB
    static constexpr uint32_t POSITION_SYNC_INTERVAL = 8 * 1024 * 1024;  // 8 MB
    const std::vector<SyncPoint>& sync_points() const { return sync_points_; }

    PositionsStore positions_store_;

private:
    std::vector<Posting> postings_;
    size_t size_bytes_{0};
    std::vector<SyncPoint> sync_points_;
    std::vector<PositionSyncPoint> position_sync_points_;
};

class Dictionary {
public:
    explicit Dictionary(size_t bucket_size_hint = (1 << 23));

    PostingList& get_or_create(const std::string& term);
    bool contains(const std::string& term) const;
    size_t size() const;

    void clear_postings();
    void iterate_terms(const std::function<void(const std::string&, const PostingList&)>& fn) const;

    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
    Dictionary(Dictionary&&) = delete;
    Dictionary& operator=(Dictionary&&) = delete;

private:
    struct Entry {
        std::string term;
        PostingList postings;
        Entry* next{nullptr};

        explicit Entry(std::string t) : term(std::move(t)) {}
    };

    size_t hash(const std::string& term) const;

    // keeping in if needed later, currently cache locality curr version is better
    // static constexpr size_t NUM_SHARDS = 8;
    // struct Shard {
    //     std::vector<Entry*> buckets;
    //     std::vector<std::unique_ptr<Entry>> entries;
    //     mutable std::mutex mutex_;
    //     std::atomic<size_t> size{0};
    //     Shard() = default;
    //     void initialize(size_t bucket_size) {
    //         buckets.resize(bucket_size/NUM_SHARDS);
    //     }
    // };
    // std::array<Shard, NUM_SHARDS> shards_;
    // size_t get_shard_index(const std::string& term) const { return (hash(term) >> 56) & (NUM_SHARDS - 1); }
    // size_t get_bucket_index(const std::string& term, size_t shard_size) const { return hash(term) % shard_size; }

    std::vector<Entry*> buckets_;
    std::vector<std::unique_ptr<Entry>> entries_;
    mutable std::mutex mutex_;
    size_t size_{0};
};
}  // namespace mithril

#endif  // INDEX_TERMSTORE_H