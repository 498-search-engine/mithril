#include "DocumentQueue.h"

#include "http/RequestExecutor.h"

#include <cassert>
#include <mutex>
#include <optional>
#include <utility>
#include <spdlog/spdlog.h>

namespace mithril {

DocumentQueue::DocumentQueue() : closed_(false) {}

void DocumentQueue::Close() {
    std::unique_lock lock(mu_);
    if (!closed_) {
        closed_ = true;
        cv_.notify_all();
    }
}

void DocumentQueue::Push(http::CompleteResponse res) {
    std::unique_lock lock(mu_);
    readyResponses_.push(std::move(res));
    SPDLOG_TRACE("document queue size increased = {}", readyResponses_.size());
    cv_.notify_one();
}

std::optional<http::CompleteResponse> DocumentQueue::Pop() {
    std::unique_lock lock(mu_);
    cv_.wait(lock, [this]() { return !this->readyResponses_.empty() || this->closed_; });

    if (this->readyResponses_.empty() && this->closed_) {
        return std::nullopt;
    }

    auto res = std::move(readyResponses_.front());
    readyResponses_.pop();
    SPDLOG_TRACE("document queue size decreased = {}", readyResponses_.size());

    return {std::move(res)};
}

}  // namespace mithril
