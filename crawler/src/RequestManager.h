#ifndef CRAWLER_REQUESTMANAGER_H
#define CRAWLER_REQUESTMANAGER_H

#include "DocumentQueue.h"
#include "UrlFrontier.h"
#include "http/RequestExecutor.h"

namespace mithril {

class RequestManager {
public:
    RequestManager(size_t targetConcurrentReqs, UrlFrontier* frontier, DocumentQueue* docQueue);

    void Run();
    void Stop();

private:
    void DispatchReadyResponse(http::ReqRes res);
    void DispatchFailedRequest(http::ReqConn req);

    size_t targetConcurrentReqs_;
    UrlFrontier* frontier_;
    DocumentQueue* docQueue_;

    http::RequestExecutor requestExecutor_;

    std::atomic<bool> stopped_;
};

}  // namespace mithril

#endif
