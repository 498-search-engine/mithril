#ifndef CRAWLER_DOCUMENTQUEUE_H
#define CRAWLER_DOCUMENTQUEUE_H

#include "http/RequestExecutor.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

namespace mithril {

class DocumentQueue {
public:
    DocumentQueue();

    void Close();

    void Push(http::CompleteResponse res);
    void PushAll(std::vector<http::CompleteResponse>& res);
    std::optional<http::CompleteResponse> Pop();

private:
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;

    bool closed_;
    std::queue<http::CompleteResponse> readyResponses_;
};

}  // namespace mithril

#endif
