#include "TermStore.h"

namespace mithril {

void PostingList::add(const Posting& posting) {
    postings_.push_back(posting);
    size_bytes_ += sizeof(Posting);
}

const std::vector<Posting>& PostingList::postings() const {
    return postings_;
}

size_t PostingList::size_bytes() const {
    return size_bytes_;
}

void PostingList::clear() {
    postings_.clear();
    size_bytes_ = 0;
}

bool PostingList::empty() const {
    return postings_.empty();
}

Dictionary::Dictionary(size_t bucket_size_hint) : buckets_(bucket_size_hint, nullptr) {
    entries_.reserve(bucket_size_hint / 2);
    for (auto& shard : shards_) {
        shard.initialize(bucket_size_hint);
    }
}

size_t Dictionary::hash(const std::string& term) const {
    static const size_t FNV_offset_basis = 14695981039346656037ULL;
    static const size_t FNV_prime = 1099511628211ULL;

    size_t hash = FNV_offset_basis;
    for (char c : term) {
        hash ^= static_cast<unsigned char>(c);
        hash *= FNV_prime;
    }
    hash ^= hash >> 32;
    return hash % buckets_.size();
}

PostingList& Dictionary::get_or_create(const std::string& term) {
    auto& shard = shards_[get_shard_index(term)];
    const size_t bucket = get_bucket_index(term, shard.buckets.size());
    
    std::lock_guard<std::mutex> lock(shard.mutex_);
    
    // Search chain
    Entry* curr = shard.buckets[bucket];
    while (curr) {
        if (curr->term == term) {
            return curr->postings;
        }
        curr = curr->next;
    }
    
    // Term not found - insert at back
    auto new_entry = std::make_unique<Entry>(term);
    
    if (!shard.buckets[bucket]) {
        shard.buckets[bucket] = new_entry.get();
    } else {
        curr = shard.buckets[bucket];
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_entry.get();
    }
    
    Entry* entry_ptr = new_entry.get();
    shard.entries.push_back(std::move(new_entry));
    shard.size++;
    
    return entry_ptr->postings;
}

size_t Dictionary::size() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        total += shard.size.load(std::memory_order_relaxed);
    }
    return total;
}

bool Dictionary::contains(const std::string& term) const {
    auto& shard = shards_[get_shard_index(term)];
    const size_t bucket = get_bucket_index(term, shard.buckets.size());
    
    std::lock_guard<std::mutex> lock(shard.mutex_);
    Entry* curr = shard.buckets[bucket];
    while (curr) {
        if (curr->term == term) return true;
        curr = curr->next;
    }
    return false;
}

void Dictionary::clear_postings() {
    for (auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard.mutex_);
        for (auto& entry : shard.entries) {
            entry->postings.clear();
        }
    }
}

void Dictionary::iterate_terms(const std::function<void(const std::string&, const PostingList&)>& fn) const {
    // Collect and sort terms from all shards
    std::vector<std::pair<std::string, const PostingList*>> all_terms;
    all_terms.reserve(size());
    
    for (const auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard.mutex_);
        for (const auto& entry : shard.entries) {
            if (!entry->postings.empty()) {
                all_terms.emplace_back(entry->term, &entry->postings);
            }
        }
    }
    
    // Sort and process
    std::sort(all_terms.begin(), all_terms.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [term, postings] : all_terms) {
        fn(term, *postings);
    }
}

}  // namespace mithril