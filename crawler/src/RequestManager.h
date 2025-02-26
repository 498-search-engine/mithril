#ifndef CRAWLER_REQUESTMANAGER_H
#define CRAWLER_REQUESTMANAGER_H

#include "DocumentQueue.h"
#include "MiddleQueue.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "http/RequestExecutor.h"

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace mithril {

class RequestManager {
public:
    RequestManager(size_t targetConcurrentReqs,
                   unsigned long requestTimeout,
                   UrlFrontier* frontier,
                   DocumentQueue* docQueue);

    void Run(ThreadSync& sync);

    void RestoreQueuedURLs(std::vector<std::string>& urls);
    void ExtractQueuedURLs(std::vector<std::string>& out);

private:
    void DispatchFailedRequest(http::FailedRequest failed);

    size_t targetConcurrentReqs_;
    unsigned long requestTimeout_;

    MiddleQueue middleQueue_;
    DocumentQueue* docQueue_;

    http::RequestExecutor requestExecutor_;
};

}  // namespace mithril

#endif
