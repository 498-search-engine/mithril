#include "TermStore.h"

#include <algorithm>

namespace mithril {

void PostingList::add(const Posting& posting) {
    postings_.push_back(posting);
    size_bytes_ += sizeof(Posting);
}

void PostingList::add(uint32_t doc_id, uint32_t freq) {
    postings_.push_back({doc_id, freq, UINT32_MAX});
    size_bytes_ += sizeof(Posting);
}

void PostingList::add_with_positions(uint32_t doc_id, uint32_t freq, const std::vector<uint32_t>& positions) {
    uint32_t pos_offset = positions_store_.all_positions.size();
    postings_.push_back({doc_id, freq, pos_offset});
    // Delta encode positions
    if (!positions.empty()) {
        uint32_t prev_pos = 0;
        uint32_t absolute_pos = 0;

        for (size_t i = 0; i < positions.size(); i++) {
            uint32_t delta = positions[i] - prev_pos;
            positions_store_.all_positions.push_back(delta);
            prev_pos = positions[i];

            // Accumulate absolute position properly
            if (i == 0)
                absolute_pos = positions[i];
            else
                absolute_pos += delta;

            if (i % POSITION_SYNC_INTERVAL == 0) {
                position_sync_points_.push_back({static_cast<uint32_t>(pos_offset + i), absolute_pos});
            }
        }
    }

    size_bytes_ += sizeof(Posting) + positions.size() * sizeof(uint32_t);
}

size_t PostingList::find_nearest_position_sync_point(uint32_t target_position) const {
    if (position_sync_points_.empty()) {
        return 0;  // No sync points, start from beginning
    }

    // binarySearch to find closest sync point less than or equal to target
    size_t left = 0;
    size_t right = position_sync_points_.size() - 1;

    // If the vector is empty or target is before first sync point
    if (position_sync_points_.empty() || position_sync_points_[0].absolute_pos > target_position)
        return 0;
    // If target is beyond last sync point
    if (position_sync_points_[right].absolute_pos <= target_position)
        return position_sync_points_[right].pos_offset;

    while (left < right) {
        size_t mid = left + (right - left + 1) / 2;
        if (position_sync_points_[mid].absolute_pos <= target_position) {
            left = mid;
        } else {
            right = mid - 1;
        }
    }

    return position_sync_points_[left].pos_offset;
}

std::vector<uint32_t> PostingList::get_positions(size_t posting_index) const {
    if (posting_index >= postings_.size())
        return {};
    const Posting& posting = postings_[posting_index];
    if (posting.positions_offset == UINT32_MAX)
        return {};

    size_t start = posting.positions_offset;
    size_t end = (posting_index == postings_.size() - 1) ? positions_store_.all_positions.size()
                                                         : postings_[posting_index + 1].positions_offset;

    // Reconstruct abs positions from deltas
    std::vector<uint32_t> absolute_positions;
    absolute_positions.reserve(end - start);
    uint32_t current_pos = 0;
    for (size_t i = start; i < end; i++) {
        current_pos += positions_store_.all_positions[i];  // Add delta
        absolute_positions.push_back(current_pos);
    }

    return absolute_positions;
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
