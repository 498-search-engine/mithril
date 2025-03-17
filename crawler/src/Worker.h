#ifndef CRAWLER_WORKER_H
#define CRAWLER_WORKER_H

#include "DocumentQueue.h"
#include "State.h"
#include "UrlFrontier.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/Response.h"

#include <string>

namespace mithril {

class Worker {
public:
    Worker(LiveState& state, DocumentQueue* docQueue, UrlFrontier* frontier_, std::string docsDirectory_);

    void Run();

private:
    void ProcessDocument(const http::Request& req, const http::Response& res, const http::ResponseHeader& header);
    void ProcessHTMLDocument(const http::Request& req, const http::Response& res, const http::ResponseHeader& header);

    LiveState& state_;
    DocumentQueue* docQueue_;
    UrlFrontier* frontier_;

    std::string docsDirectory_;

    html::ParsedDocument parsedDoc_;
};

}  // namespace mithril

#endif
