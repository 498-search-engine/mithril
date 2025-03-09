#include "Worker.h"

#include "Clock.h"
#include "CrawlerMetrics.h"
#include "DocumentQueue.h"
#include "State.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "data/Document.h"
#include "data/Gzip.h"
#include "data/Serialize.h"
#include "data/Writer.h"
#include "html/Link.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"

#include <atomic>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

namespace {

void WriteDocumentToFile(const std::string& fileName, const data::Document& doc) {
    auto fWriter = data::FileWriter{fileName.c_str()};
    auto zipWriter = data::GzipWriter{fWriter};
    data::SerializeValue(doc, zipWriter);
    zipWriter.Finish();
}

}  // namespace

Worker::Worker(LiveState& state, DocumentQueue* docQueue, UrlFrontier* frontier, std::string docsDirectory)
    : state_(state), docQueue_(docQueue), frontier_(frontier), docsDirectory_(std::move(docsDirectory)) {}

void Worker::Run() {
    spdlog::info("worker starting");
    while (!state_.threadSync.ShouldShutdown()) {
        state_.threadSync.MaybePause();
        auto doc = docQueue_->Pop();
        if (!doc) {
            continue;
        }

        auto start = MonotonicTimeMs();
        ProcessDocument(doc->req, doc->res, doc->header);
        auto end = MonotonicTimeMs();
        spdlog::debug("worker took {} ms to process document {} ({} bytes)",
                      end - start,
                      doc->req.Url().url,
                      doc->res.body.size());
    }
    spdlog::info("worker terminating");
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
    if (!parser.titleWords.empty()) {
        for (auto& w : parser.titleWords) {
            title.append(w);
            title.push_back(' ');
        }
        title.pop_back();
    }

    spdlog::info("processing {} ({})", req.Url().url, title);

    std::vector<std::string> absoluteURLs;
    for (auto& l : parser.links) {
        auto absoluteLink = html::MakeAbsoluteLink(req.Url(), parser.base, l.URL);
        if (absoluteLink) {
            absoluteURLs.push_back(std::move(*absoluteLink));
        }
    }

    data::docid_t docID = state_.nextDocumentID.fetch_add(1);
    auto idStr = std::to_string(docID);
    auto fileName = docsDirectory_ + "/doc_";
    for (int i = 0; i < 10 - idStr.size(); ++i) {
        fileName.push_back('0');
    }
    fileName.append(idStr);

    WriteDocumentToFile(fileName,
                        data::Document{
                            .id = docID,
                            .url = req.Url().url,
                            .title = std::move(parser.titleWords),
                            .words = std::move(parser.words),
                        });

    if (!absoluteURLs.empty()) {
        frontier_->PushURLs(absoluteURLs);
        absoluteURLs.clear();
    }
}

void Worker::ProcessDocument(const http::Request& req, const http::Response& res, const http::ResponseHeader& header) {
    DocumentsProcessedMetric.Inc();
    CrawlResponseCodesMetric
        .WithLabels({
            {"status", std::to_string(header.status)}
    })
        .Inc();

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

            frontier_->PushURL(std::move(*newUrl));
            break;
        }

    default:
        spdlog::info("unhandled status {} for {}", header.status, req.Url().url);
        break;
    }
}

}  // namespace mithril
