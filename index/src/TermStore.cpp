#include "TermStore.h"

#include <algorithm>

namespace mithril {

void PostingList::add(const Posting& posting) {
    postings_.push_back(posting);
    size_bytes_ += sizeof(Posting);
}

void PostingList::add(uint32_t doc_id, uint32_t freq) {
    postings_.push_back({doc_id, freq});
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
    const size_t bucket = hash(term);
    std::lock_guard<std::mutex> lock(mutex_);

    // Search chain
    Entry* curr = buckets_[bucket];
    while (curr) {
        if (curr->term == term) {
            return curr->postings;
        }
        curr = curr->next;
    }

    // Term not found - insert at back
    auto new_entry = std::make_unique<Entry>(term);

    if (!buckets_[bucket]) {
        // First entry in bucket
        buckets_[bucket] = new_entry.get();
    } else {
        // Find end of chain and insert
        curr = buckets_[bucket];
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_entry.get();
    }

    Entry* entry_ptr = new_entry.get();
    entries_.push_back(std::move(new_entry));
    size_++;

    return entry_ptr->postings;
}

bool Dictionary::contains(const std::string& term) const {
    const size_t bucket = hash(term);
    std::lock_guard<std::mutex> lock(mutex_);

    Entry* curr = buckets_[bucket];
    while (curr) {
        if (curr->term == term) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}

size_t Dictionary::size() const {
    return size_;
}

void Dictionary::clear_postings() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : entries_) {
        entry->postings.clear();
    }
}

void Dictionary::iterate_terms(const std::function<void(const std::string&, const PostingList&)>& fn) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create sorted list of terms
    std::vector<const Entry*> sorted_entries;
    sorted_entries.reserve(entries_.size());

    for (const auto& entry : entries_) {
        if (!entry->postings.empty()) {
            sorted_entries.push_back(entry.get());
        }
    }

    // Sort by term
    std::sort(
        sorted_entries.begin(), sorted_entries.end(), [](const Entry* a, const Entry* b) { return a->term < b->term; });

    // Call function for each term
    for (const Entry* entry : sorted_entries) {
        fn(entry->term, entry->postings);
    }
}

}  // namespace mithril
