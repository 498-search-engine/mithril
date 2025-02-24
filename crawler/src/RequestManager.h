#ifndef CRAWLER_REQUESTMANAGER_H
#define CRAWLER_REQUESTMANAGER_H

#include "DocumentQueue.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "http/RequestExecutor.h"

#include <atomic>
#include <cstddef>

namespace mithril {

class RequestManager {
public:
    RequestManager(size_t targetConcurrentReqs,
                   unsigned long requestTimeout,
                   UrlFrontier* frontier,
                   DocumentQueue* docQueue);

    void Run(ThreadSync& sync);
    void Stop();

private:
    void DispatchFailedRequest(http::FailedRequest failed);

    size_t targetConcurrentReqs_;
    unsigned long requestTimeout_;

    UrlFrontier* frontier_;
    DocumentQueue* docQueue_;

    http::RequestExecutor requestExecutor_;

    std::atomic<bool> stopped_;
};

}  // namespace mithril

#endif
