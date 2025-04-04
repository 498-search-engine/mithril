#ifndef CRAWLER_REQUESTMANAGER_H
#define CRAWLER_REQUESTMANAGER_H

#include "Config.h"
#include "DocumentQueue.h"
#include "HostRateLimiter.h"
#include "MiddleQueue.h"
#include "StringTrie.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "http/RequestExecutor.h"

#include <cstddef>
#include <string>
#include <vector>

namespace mithril {

class RequestManager {
public:
    RequestManager(UrlFrontier* frontier,
                   HostRateLimiter* limiter,
                   DocumentQueue* docQueue,
                   const CrawlerConfig& config,
                   const StringTrie& blacklistedHosts);

    void Run(ThreadSync& sync);
    void TouchRequestTimeouts();

    void RestoreQueuedURLs(std::vector<std::string>& urls);
    void DumpQueuedURLs(std::vector<std::string>& out);

private:
    void DispatchFailedRequest(const http::FailedRequest& failed);

    size_t targetConcurrentReqs_;
    unsigned long requestTimeout_;

    MiddleQueue middleQueue_;
    HostRateLimiter* limiter_;
    DocumentQueue* docQueue_;
    const StringTrie& blacklistedHosts_;

    http::RequestExecutor requestExecutor_;
};

}  // namespace mithril

#endif
