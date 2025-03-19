#ifndef CRAWLER_WORKER_H
#define CRAWLER_WORKER_H

#include "DocumentQueue.h"
#include "State.h"
#include "UrlFrontier.h"
#include "core/optional.h"
#include "data/Document.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/Response.h"

#include <set>
#include <string>
#include <utility>

namespace mithril {

constexpr auto DocumentChunkSize = 10000;

class Worker {
public:
    Worker(LiveState& state,
           DocumentQueue* docQueue,
           UrlFrontier* frontier,
           std::string docsDirectory,
           const std::set<std::string>& blacklistedHosts);

    void Run();

private:
    void ProcessDocument(const http::Request& req, http::Response& res);
    void ProcessHTMLDocument(const http::Request& req, const http::Response& res);

    std::pair<data::docid_t, std::string> NextDocument();

    LiveState& state_;
    DocumentQueue* docQueue_;
    UrlFrontier* frontier_;

    std::string docsDirectory_;
    const std::set<std::string>& blacklistedHosts_;

    html::ParsedDocument parsedDoc_;
    core::Optional<data::docid_t> lastChunk_;
};

}  // namespace mithril

#endif
