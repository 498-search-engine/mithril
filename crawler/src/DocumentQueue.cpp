#include "DocumentQueue.h"

#include "CrawlerMetrics.h"
#include "ThreadSync.h"
#include "core/locks.h"
#include "http/RequestExecutor.h"

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

DocumentQueue::DocumentQueue(ThreadSync& sync) : sync_(sync) {
    sync_.RegisterCV(&cv_);
}

void DocumentQueue::Push(http::CompleteResponse res) {
    core::LockGuard lock(mu_);
    readyResponses_.push(std::move(res));
    cv_.Signal();
    DocumentQueueSizeMetric.Set(readyResponses_.size());
}

void DocumentQueue::PushAll(std::vector<http::CompleteResponse>& res) {
    core::LockGuard lock(mu_);
    for (auto& r : res) {
        readyResponses_.push(std::move(r));
    }
    cv_.Broadcast();
    DocumentQueueSizeMetric.Set(readyResponses_.size());
}

std::optional<http::CompleteResponse> DocumentQueue::Pop() {
    core::LockGuard lock(mu_);
    cv_.Wait(lock, [this]() { return !this->readyResponses_.empty() || sync_.ShouldSynchronize(); });
    if (sync_.ShouldSynchronize() || this->readyResponses_.empty()) {
        return std::nullopt;
    }

    auto res = std::move(readyResponses_.front());
    readyResponses_.pop();
    DocumentQueueSizeMetric.Set(readyResponses_.size());

    return {std::move(res)};
}

void DocumentQueue::DumpCompletedURLs(std::vector<std::string>& out) {
    core::LockGuard lock(mu_);

    size_t n = readyResponses_.size();
    for (size_t i = 0; i < n; ++i) {
        auto front = std::move(readyResponses_.front());
        out.push_back(front.req.Url().url);
        readyResponses_.pop();
        readyResponses_.push(std::move(front));
    }
    assert(readyResponses_.size() == n);
}

}  // namespace mithril
