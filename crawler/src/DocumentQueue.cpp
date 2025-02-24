#include "DocumentQueue.h"

#include "ThreadSync.h"
#include "core/locks.h"
#include "http/RequestExecutor.h"

#include <cassert>
#include <optional>
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
    SPDLOG_TRACE("document queue size increased = {}", readyResponses_.size());
    cv_.Signal();
}

void DocumentQueue::PushAll(std::vector<http::CompleteResponse>& res) {
    core::LockGuard lock(mu_);
    for (auto& r : res) {
        SPDLOG_DEBUG("ready document added to queue: {}", r.req.Url().url);
        readyResponses_.push(std::move(r));
    }
    SPDLOG_TRACE("document queue size increased = {}", readyResponses_.size());
    cv_.Broadcast();
}

std::optional<http::CompleteResponse> DocumentQueue::Pop() {
    core::LockGuard lock(mu_);
    cv_.Wait(lock, [this]() { return !this->readyResponses_.empty() || sync_.ShouldSynchronize(); });
    if (sync_.ShouldSynchronize() || this->readyResponses_.empty()) {
        return std::nullopt;
    }

    auto res = std::move(readyResponses_.front());
    readyResponses_.pop();
    SPDLOG_TRACE("document queue size decreased = {}", readyResponses_.size());

    return {std::move(res)};
}

}  // namespace mithril
