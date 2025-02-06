#ifndef CRAWLER_DOCUMENTQUEUE_H
#define CRAWLER_DOCUMENTQUEUE_H

#include "http/RequestExecutor.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace mithril {

class DocumentQueue {
public:
    DocumentQueue();

    void Close();

    void Push(http::ReqRes res);
    std::optional<http::ReqRes> Pop();

private:
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;

    bool closed_;
    std::queue<http::ReqRes> readyResponses_;
};

}  // namespace mithril

#endif
