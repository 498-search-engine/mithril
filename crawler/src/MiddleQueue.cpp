#include "MiddleQueue.h"

#include "Clock.h"
#include "Config.h"
#include "CrawlerMetrics.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "core/memory.h"
#include "core/optional.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unistd.h>
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
                         unsigned long defaultCrawlDelayMs)
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

void MiddleQueue::DumpQueuedURLs(std::vector<std::string>& out) {
    for (const auto& entry : hosts_) {
        auto cpy = entry.second->queue;
        while (!cpy.empty()) {
            out.push_back(std::move(cpy.front()));
            cpy.pop();
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

        bool wait = atLeastOne && ActiveQueueCount() == 0;

        // Get URLs from the frontier that match the WantURL() predicate
        frontier_->GetURLsFiltered(
            sync, totalTargetQueuedURLs, r, [this](std::string_view url) { return WantURL(url); }, wait);
        if (sync.ShouldSynchronize()) {
            return;
        }

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

    if (ActiveQueueCount() > 0) {
        auto waitDuration = std::numeric_limits<long>::max();
        bool anyReady = false;

        for (size_t i = 0; i < n_; ++i, k_ = (k_ + 1) % n_) {
            auto* record = queues_[k_];
            if (record == nullptr || record->queue.empty()) {
                continue;
            }

            if (record->waitingDelayLookup) {
                auto delay = frontier_->LookUpCrawlDelayNonblocking(record->host, 0);
                if (delay.HasValue()) {
                    record->waitingDelayLookup = false;
                    record->crawlDelayMs = CrawlDelayFromDirective(*delay);
                } else {
                    // Still waiting
                    continue;
                }
            }

            if (now < record->earliestNextCrawl) {
                waitDuration = std::min(waitDuration, record->earliestNextCrawl - now);
                continue;
            }

            anyReady = true;
            out.push_back(PopFromHost(now, *record));
            ++added;
            if (added >= maxPossibleReady) {
                k_ = (k_ + 1) % n_;
                break;
            }
        }

        if (!anyReady && atLeastOne && waitDuration != std::numeric_limits<long>::max()) {
            // We are waiting for the next delayed crawl to be ready.
            usleep(waitDuration * 1000 / 2);
        }
    }

    MiddleQueueActiveQueueCount.Set(ActiveQueueCount());
    MiddleQueueTotalQueuedURLs.Set(totalQueuedURLs_);
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
    auto canonicalHost = http::CanonicalizeHost(*parsed);

    auto recordIt = hosts_.find(canonicalHost.url);
    if (recordIt != hosts_.end()) {
        PushURLForHost(std::move(url), recordIt->second.Get());
    } else {
        PushURLForNewHost(now, std::move(url), canonicalHost);
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

void MiddleQueue::PushURLForNewHost(long now, std::string url, const http::CanonicalHost& host) {
    auto record = core::MakeUnique<HostRecord>(HostRecord{
        .host = host,
        .waitingDelayLookup = true,
        .crawlDelayMs = defaultCrawlDelayMs_,
        .earliestNextCrawl = now,
        .queue = {},
        .activeQueue = {},
    });

    auto delay = frontier_->LookUpCrawlDelayNonblocking(host, 0);
    if (delay.HasValue()) {
        record->waitingDelayLookup = false;
        record->crawlDelayMs = CrawlDelayFromDirective(*delay);
    }

    auto it = hosts_.insert({
        host.url,
        std::move(record),
    });
    assert(it.second);

    PushURLForHost(std::move(url), it.first->second.Get());
}

std::string MiddleQueue::PopFromHost(long now, HostRecord& record) {
    assert(!record.queue.empty());
    assert(!record.waitingDelayLookup);
    assert(record.earliestNextCrawl <= now);
    assert(record.activeQueue.HasValue());

    auto url = std::move(record.queue.front());
    record.queue.pop();
    --totalQueuedURLs_;

    record.earliestNextCrawl = static_cast<long>(static_cast<unsigned long>(now) + record.crawlDelayMs);
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
    auto now = MonotonicTimeMs();
    for (auto it = hosts_.begin(); it != hosts_.end();) {
        if (it->second->queue.empty() && now >= it->second->earliestNextCrawl + it->second->crawlDelayMs) {
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


unsigned long MiddleQueue::CrawlDelayFromDirective(unsigned long directive) const {
    // Clamp between default and 60 seconds
    return std::clamp(directive * 1000L, defaultCrawlDelayMs_, 60UL * 1000UL);
}

}  // namespace mithril
