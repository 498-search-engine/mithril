#include "DocumentQueue.h"

#include "http/RequestExecutor.h"

#include <cassert>
#include <mutex>

namespace mithril {

DocumentQueue::DocumentQueue() : closed_(false) {}

void DocumentQueue::Close() {
    std::unique_lock lock(mu_);
    if (!closed_) {
        closed_ = true;
        cv_.notify_all();
    }
}

void DocumentQueue::Push(http::ReqRes res) {
    std::unique_lock lock(mu_);
    readyResponses_.push(std::move(res));
    cv_.notify_one();
}

std::optional<http::ReqRes> DocumentQueue::Pop() {
    std::unique_lock lock(mu_);
    cv_.wait(lock, [this]() { return !this->readyResponses_.empty() || this->closed_; });

    if (this->readyResponses_.empty() && this->closed_) {
        return std::nullopt;
    }

    auto res = std::move(readyResponses_.front());
    readyResponses_.pop();

    return {std::move(res)};
}

}  // namespace mithril
