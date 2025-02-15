#include "Worker.h"

#include "Clock.h"
#include "DocumentQueue.h"
#include "UrlFrontier.h"
#include "html/Link.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"

#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

Worker::Worker(DocumentQueue* docQueue, UrlFrontier* frontier) : docQueue_(docQueue), frontier_(frontier) {}

void Worker::Run() {
    spdlog::info("worker starting");
    while (true) {
        auto doc = docQueue_->Pop();
        if (!doc) {
            // Closed
            spdlog::info("worker terminating");
            return;
        }

        auto start = MonotonicTimeMs();
        ProcessDocument(doc->req, doc->res, doc->header);
        auto end = MonotonicTimeMs();
        spdlog::debug("worker took {} ms to process document {} ({} bytes)",
                      end - start,
                      doc->req.Url().url,
                      doc->res.body.size());
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
    std::string title;
    for (auto& w : parser.titleWords) {
        title.append(w);
        title.push_back(' ');
    }
    title.pop_back();

    spdlog::info("processing {} ({})", req.Url().url, title);

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
            SPDLOG_TRACE("missing content-type header for {}", req.Url().url);
            return;
        }
        if (header.ContentType->value.starts_with("text/html")) {
            ProcessHTMLDocument(req, res, header);
        } else {
            spdlog::debug("unsupported content-type {} for {}", header.ContentType->value, req.Url().url);
        }
        break;

    case http::StatusCode::MovedPermanently:
    case http::StatusCode::Found:
    case http::StatusCode::SeeOther:
    case http::StatusCode::TemporaryRedirect:
    case http::StatusCode::PermanentRedirect:
        {
            if (!header.Location) {
                return;
            }

            std::string location{header.Location->value};
            auto newUrl = html::MakeAbsoluteLink(req.Url(),
                                                 "",  // No base tag for redirects
                                                 location);

            if (!newUrl) {
                return;
            }

            frontier_->PutURL(std::move(*newUrl));
            break;
        }

    default:
        spdlog::info("unhandled status {} for {}", header.status, req.Url().url);
        break;
    }
}

}  // namespace mithril
