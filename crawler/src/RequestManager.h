#ifndef CRAWLER_REQUESTMANAGER_H
#define CRAWLER_REQUESTMANAGER_H

#include "Config.h"
#include "DocumentQueue.h"
#include "MiddleQueue.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "http/RequestExecutor.h"

#include <cstddef>
#include <string>
#include <vector>

namespace mithril {

class RequestManager {
public:
    RequestManager(UrlFrontier* frontier, DocumentQueue* docQueue, const CrawlerConfig& config);

    void Run(ThreadSync& sync);

    void RestoreQueuedURLs(std::vector<std::string>& urls);
    void DumpQueuedURLs(std::vector<std::string>& out);

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
