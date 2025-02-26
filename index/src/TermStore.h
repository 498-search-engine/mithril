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
    const std::vector<Posting>& postings() const;
    size_t size_bytes() const;
    void clear();
    bool empty() const;
    static constexpr uint32_t SYNC_INTERVAL = 1024;
    const std::vector<SyncPoint>& sync_points() const { return sync_points_; }

private:
    std::vector<Posting> postings_;
    std::vector<SyncPoint> sync_points_;
    size_t size_bytes_{0};
};

class Dictionary {
public:
    explicit Dictionary(size_t bucket_size_hint = (1 << 20));

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