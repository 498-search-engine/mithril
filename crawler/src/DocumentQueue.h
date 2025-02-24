#ifndef CRAWLER_DOCUMENTQUEUE_H
#define CRAWLER_DOCUMENTQUEUE_H

#include "ThreadSync.h"
#include "core/cv.h"
#include "core/mutex.h"
#include "http/RequestExecutor.h"

#include <optional>
#include <queue>
#include <vector>

namespace mithril {

class DocumentQueue {
public:
    DocumentQueue(ThreadSync& sync);

    void Push(http::CompleteResponse res);
    void PushAll(std::vector<http::CompleteResponse>& res);
    std::optional<http::CompleteResponse> Pop();

private:
    ThreadSync& sync_;

    mutable core::Mutex mu_;
    mutable core::cv cv_;

    std::queue<http::CompleteResponse> readyResponses_;
};

}  // namespace mithril

#endif
