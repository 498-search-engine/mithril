#include "Worker.h"

#include "DocumentQueue.h"
#include "UrlFrontier.h"
#include "html/Link.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"

#include <iostream>
#include <vector>

namespace mithril {

Worker::Worker(DocumentQueue* docQueue, UrlFrontier* frontier) : docQueue_(docQueue), frontier_(frontier) {}

void Worker::Run() {
    while (true) {
        auto doc = docQueue_->Pop();
        if (!doc) {
            // Closed
            std::cout << "worker terminating" << std::endl;
            return;
        }

        ProcessDocument(std::move(doc->req), std::move(doc->res));
    }
}

void Worker::ProcessDocument(http::Request req, http::Response res) {
    auto header = http::ParseResponseHeader(res);
    if (!header) {
        std::cout << "failed to parse doc " << req.Url().url << std::endl;
        return;
    }

    if (header->status != http::StatusCode::OK) {
        std::cout << "unhandled status " << header->status << std::endl;
        return;
    }

    html::Parser parser(res.body.data(), res.body.size());

    std::cout << req.Url().url << " ";
    for (auto& w : parser.titleWords) {
        std::cout << w << " ";
    }
    std::cout << std::endl << std::endl;

    std::vector<std::string> absoluteURLs;
    for (auto& l : parser.links) {
        auto absoluteLink = html::MakeAbsoluteLink(req.Url(), parser.base, l.URL);
        if (absoluteLink) {
            absoluteURLs.push_back(std::move(*absoluteLink));
        }
    }

    if (!absoluteURLs.empty()) {
        auto n = frontier_->PutURLs(std::move(absoluteURLs));
        std::cout << "pushed " << n << " links to frontier" << std::endl;
    }
}

}  // namespace mithril
