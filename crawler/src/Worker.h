#ifndef CRAWLER_WORKER_H
#define CRAWLER_WORKER_H

#include "DocumentQueue.h"
#include "UrlFrontier.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"

namespace mithril {

class Worker {
public:
    Worker(DocumentQueue* docQueue, UrlFrontier* frontier_);

    void Run();

private:
    void ProcessDocument(const http::Request& req, const http::Response& res, const http::ResponseHeader& header);
    void ProcessHTMLDocument(const http::Request& req, const http::Response& res, const http::ResponseHeader& header);

    DocumentQueue* docQueue_;
    UrlFrontier* frontier_;
};

}  // namespace mithril

#endif
