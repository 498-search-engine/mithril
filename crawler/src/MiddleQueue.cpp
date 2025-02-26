#include "MiddleQueue.h"

#include "Clock.h"
#include "Config.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "core/memory.h"
#include "core/optional.h"
#include "http/URL.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mithril {

MiddleQueue::MiddleQueue(UrlFrontier* frontier, const CrawlerConfig& config)
    : MiddleQueue(frontier,
                  config.middle_queue_queue_count,
                  config.middle_queue_url_batch_size,
                  config.middle_queue_host_url_limit,
                  config.middle_queue_utilization_target,
                  config.default_crawl_delay_ms) {}

MiddleQueue::MiddleQueue(UrlFrontier* frontier,
                         size_t numQueues,
                         size_t urlBatchSize,
                         size_t hostUrlLimit,
                         double queueUtilizationTarget,
                         long defaultCrawlDelayMs)
    : frontier_(frontier),
      n_(numQueues),
      urlBatchSize_(urlBatchSize),
      hostUrlLimit_(hostUrlLimit),
      queueUtilizationTarget_(queueUtilizationTarget),
      defaultCrawlDelayMs_(defaultCrawlDelayMs) {
    queues_.resize(numQueues, nullptr);
    emptyQueues_.resize(numQueues, 0);
    for (size_t i = 0; i < numQueues; ++i) {
        emptyQueues_[i] = numQueues - i - 1;
    }
}

void MiddleQueue::RestoreFrom(std::vector<std::string>& urls) {
    long now = MonotonicTimeMs();
    for (auto& url : urls) {
        AcceptURL(now, std::move(url));
    }
}

void MiddleQueue::ExtractQueuedURLs(std::vector<std::string>& out) {
    for (const auto& entry : hosts_) {
        while (!entry.second->queue.empty()) {
            out.push_back(std::move(entry.second->queue.front()));
            entry.second->queue.pop();
        }
    }
}

void MiddleQueue::GetURLs(ThreadSync& sync, size_t max, std::vector<std::string>& out, bool atLeastOne) {
    long now;

    auto totalTargetQueuedURLs = n_ * urlBatchSize_;
    auto utilization = QueueUtilization();
    if (totalQueuedURLs_ < totalTargetQueuedURLs || utilization < queueUtilizationTarget_) {
        if (utilization < queueUtilizationTarget_) {
            // This doesn't happen that frequently. Take this opportunity to
            // clean up any empty hosts.
            CleanEmptyHosts();
        }

        std::vector<std::string> r;
        r.reserve(totalTargetQueuedURLs);

        // Get URLs from the frontier that match the WantURL() predicate
        frontier_->GetURLsFiltered(
            sync, totalTargetQueuedURLs, r, [this](std::string_view url) { return WantURL(url); }, atLeastOne);
        if (sync.ShouldSynchronize()) {
            return;
        }

        spdlog::debug(
            "middle queue: got {} out of {} max atLeastOne = {}", r.size(), totalTargetQueuedURLs, atLeastOne);

        // Push all obtained URLs into the middle queue
        now = MonotonicTimeMs();
        for (auto url : r) {
            AcceptURL(now, std::move(url));
        }
    }

    now = MonotonicTimeMs();

    // Try to return up to max URLs by going round-robin through the active
    // queue set. A queue is only popped from if the time since the last crawl
    // is acceptable.
    size_t maxPossibleReady = std::min(max, n_);
    size_t added = 0;
    for (size_t i = 0; i < n_; ++i, k_ = (k_ + 1) % n_) {
        auto* record = queues_[k_];
        if (record == nullptr) {
            continue;
        }

        if (record->queue.empty() || now < record->earliestNextCrawl) {
            continue;
        }

        out.push_back(PopFromHost(now, *record));
        ++added;
        if (added >= maxPossibleReady) {
            k_ = (k_ + 1) % n_;
            break;
        }
    }
}

size_t MiddleQueue::ActiveQueueCount() const {
    return n_ - emptyQueues_.size();
}

double MiddleQueue::QueueUtilization() const {
    return static_cast<double>(ActiveQueueCount()) / static_cast<double>(n_);
}

void MiddleQueue::AcceptURL(long now, std::string url) {
    auto parsed = http::ParseURL(url);
    if (!parsed) {
        return;
    }
    auto host = http::CanonicalizeHost(*parsed).url;

    auto recordIt = hosts_.find(host);
    if (recordIt != hosts_.end()) {
        PushURLForHost(std::move(url), recordIt->second.Get());
    } else {
        PushURLForNewHost(now, std::move(url), std::move(host));
    }
}

void MiddleQueue::PushURLForHost(std::string url, HostRecord* record) {
    record->queue.push(std::move(url));
    ++totalQueuedURLs_;

    if (!record->activeQueue.HasValue() && !emptyQueues_.empty()) {
        // Have a empty queue, assign this host to the queue
        AssignFreeQueue(record);
    }
}

void MiddleQueue::PushURLForNewHost(long now, std::string url, std::string host) {
    auto record = core::UniquePtr<HostRecord>{
        new HostRecord{
                       .host = host,
                       .crawlDelayMs = defaultCrawlDelayMs_,
                       .earliestNextCrawl = now,
                       .queue = {},
                       .activeQueue = {},
                       }
    };

    auto it = hosts_.insert({
        std::move(host),
        std::move(record),
    });
    assert(it.second);

    PushURLForHost(std::move(url), it.first->second.Get());
}

std::string MiddleQueue::PopFromHost(long now, HostRecord& record) {
    assert(!record.queue.empty());
    assert(record.earliestNextCrawl <= now);
    assert(record.activeQueue.HasValue());

    auto url = std::move(record.queue.front());
    record.queue.pop();
    --totalQueuedURLs_;

    record.earliestNextCrawl = now + record.crawlDelayMs;
    if (record.queue.empty()) {
        // The host's queue is now empty, remove it from the active queue set
        queues_[*record.activeQueue] = nullptr;
        emptyQueues_.push_back(*record.activeQueue);
        record.activeQueue = core::nullopt;
        PopulateActiveQueues();
    }

    return url;
}

void MiddleQueue::PopulateActiveQueues() {
    size_t available = emptyQueues_.size();
    auto it = hosts_.begin();
    while (available > 0 && it != hosts_.end()) {
        if (it->second->activeQueue.HasValue() || it->second->queue.empty()) {
            ++it;
            continue;
        }
        AssignFreeQueue(it->second.Get());
        --available;
        ++it;
    }
}

void MiddleQueue::CleanEmptyHosts() {
    for (auto it = hosts_.begin(); it != hosts_.end();) {
        if (it->second->queue.empty()) {
            assert(!it->second->activeQueue.HasValue());
            it = hosts_.erase(it);
        } else {
            ++it;
        }
    }
}

void MiddleQueue::AssignFreeQueue(HostRecord* record) {
    assert(!emptyQueues_.empty());
    auto emptyQueueNum = emptyQueues_.back();
    emptyQueues_.pop_back();

    queues_[emptyQueueNum] = record;
    record->activeQueue = {emptyQueueNum};
}

bool MiddleQueue::WantURL(std::string_view url) const {
    // TODO: this may be a bit slow. How can we improve this?
    auto parsed = http::ParseURL(url);
    if (!parsed) {
        return true;
    }
    auto host = http::CanonicalizeHost(*parsed).url;
    if (auto it = hosts_.find(host); it != hosts_.end()) {
        return it->second->queue.size() < hostUrlLimit_;
    }
    return true;
}

}  // namespace mithril
