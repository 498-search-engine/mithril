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

        ProcessDocument(doc->req, doc->res, doc->header);
    }
}

/**
 * Process an HTML document that's already been validated.
 * Preconditions:
 * - Response has valid header
 * - Status code is 200 OK
 * - Content-Type is text/html
 */
void Worker::ProcessHTMLDocument(const http::Request& req,
                                 const http::Response& res,
                                 const http::ResponseHeader& header) {
    // TODO: early return if non latin script page
    html::Parser parser(res.body.data(), res.body.size());

    // TODO: do better logging, tracking
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
        frontier_->PutURLs(std::move(absoluteURLs));
    }
}

void Worker::ProcessDocument(const http::Request& req, const http::Response& res, const http::ResponseHeader& header) {
    switch (header.status) {
    case http::StatusCode::OK:
        if (!header.ContentType) {
            std::cout << "no content-type header" << std::endl;
            return;
        }
        if (header.ContentType->value.starts_with("text/html")) {
            ProcessHTMLDocument(req, res, header);
        } else {
            std::cout << "unsupported content-type: " << header.ContentType->value << std::endl;
        }
        break;

    case http::StatusCode::MovedPermanently:
    case http::StatusCode::Found:
    case http::StatusCode::SeeOther:
    case http::StatusCode::TemporaryRedirect:
    case http::StatusCode::PermanentRedirect:
        {
            if (!header.Location) {
                std::cout << "redirect without Location header: " << req.Url().url << std::endl;
                return;
            }

            std::string location{header.Location->value};
            auto new_url = html::MakeAbsoluteLink(req.Url(),
                                                  "",  // No base tag for redirects
                                                  location);

            if (!new_url) {
                std::cout << "invalid redirect Location: '" << location << "' from " << req.Url().url << std::endl;
                return;
            }

            frontier_->PutURL(std::move(*new_url));
            break;
        }

    default:
        std::cout << "unhandled status " << header.status << std::endl;
        break;
    }
}

}  // namespace mithril
