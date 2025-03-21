#ifndef CRAWLER_WORKER_H
#define CRAWLER_WORKER_H

#include "DocumentQueue.h"
#include "State.h"
#include "StringTrie.h"
#include "UrlFrontier.h"
#include "core/optional.h"
#include "data/Document.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/Response.h"
#include "http/URL.h"

#include <string>
#include <utility>
#include <vector>

namespace mithril {

constexpr auto DocumentChunkSize = 10000;

class Worker {
public:
    Worker(LiveState& state,
           DocumentQueue* docQueue,
           UrlFrontier* frontier,
           std::string docsDirectory,
           const StringTrie& blacklistedHosts);

    void Run();

private:
    void ProcessDocument(const http::Request& req, http::Response& res);
    void ProcessHTMLDocument(const http::Request& req, const http::Response& res);

    void SaveDocument(data::DocumentView doc);

    std::pair<data::docid_t, std::string> NextDocument();
    std::vector<std::string> GetFollowURLs(const html::ParsedDocument& doc, const http::URL& url) const;

    LiveState& state_;
    DocumentQueue* docQueue_;
    UrlFrontier* frontier_;

    std::string docsDirectory_;
    const StringTrie& blacklistedHosts_;

    html::ParsedDocument parsedDoc_;
    core::Optional<data::docid_t> lastChunk_;
};

}  // namespace mithril

#endif
