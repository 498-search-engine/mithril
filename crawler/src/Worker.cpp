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
#include "http/URL.h"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <sys/stat.h>

namespace mithril {

namespace {

void WriteDocumentToFile(const std::string& fileName, const data::DocumentView& doc) {
    try {
        auto fWriter = data::FileWriter{fileName.c_str()};
        auto zipWriter = data::GzipWriter{fWriter};
        data::SerializeValue(doc, zipWriter);
        zipWriter.Finish();
    } catch (const std::exception& e) {
        spdlog::error("failed to write document {} ({}): {}", doc.id, doc.url, e.what());
    }
}

std::string NumberedEntity(std::string_view entity, size_t num, int pad) {
    std::string res;
    res.reserve(entity.size() + pad + 1);

    res.append(entity);
    res.push_back('_');
    auto numStr = std::to_string(num);
    for (int i = 0; i < pad - numStr.size(); ++i) {
        res.push_back('0');
    }
    res.append(numStr);
    return res;
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
        ProcessDocument(doc->req, doc->res);
        auto end = MonotonicTimeMs();
        auto elapsedMs = end - start;

        SPDLOG_DEBUG(
            "worker took {} ms to process document {} ({} bytes)", elapsedMs, doc->req.Url().url, doc->res.body.size());
        DocumentProcessDurationMetric.Observe(static_cast<double>(elapsedMs) / 1000.0);
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
void Worker::ProcessHTMLDocument(const http::Request& req, const http::Response& res) {
    // TODO: early return if non latin script page
    html::ParseDocument(std::string_view{res.body.data(), res.body.size()}, parsedDoc_);

    // TODO: do better logging, tracking
    std::string title;
    if (!parsedDoc_.titleWords.empty()) {
        for (auto& w : parsedDoc_.titleWords) {
            title.append(w);
            title.push_back(' ');
        }
        title.pop_back();
    }

    spdlog::info("processing {} ({})", req.Url().url, title);

    std::vector<std::string> absoluteURLs;
    for (auto& l : parsedDoc_.links) {
        auto absoluteLink = html::MakeAbsoluteLink(req.Url(), parsedDoc_.base, l.url);
        if (!absoluteLink) {
            continue;
        }

        auto parsed = http::ParseURL(*absoluteLink);
        if (!parsed) {
            continue;
        }

        auto canonical = http::CanonicalizeURL(*parsed);
        absoluteURLs.push_back(std::move(canonical));
    }

    auto [docID, docPath] = NextDocument();
    if (docPath.empty()) {
        spdlog::error("failed to get next document path");
        return;
    }

    WriteDocumentToFile(docPath,
                        data::DocumentView{
                            .id = docID,
                            .url = req.Url().url,
                            .title = parsedDoc_.titleWords,
                            .words = parsedDoc_.words,
                        });

    if (!absoluteURLs.empty()) {
        frontier_->PushURLs(absoluteURLs);
        absoluteURLs.clear();
    }

    DocumentSizeBytesMetric.Observe(res.body.size());
}

void Worker::ProcessDocument(const http::Request& req, http::Response& res) {
    DocumentsProcessedMetric.Inc();
    CrawlResponseCodesMetric
        .WithLabels({
            {"status", std::to_string(res.header.status)}
    })
        .Inc();

    try {
        // First, decode the body if it is encoded.
        res.DecodeBody();
    } catch (const std::exception& e) {
        // Something went wrong while decoding
        spdlog::warn("encountered error while decoding body for {}: {}", req.Url().url, e.what());
        return;
    }

    switch (res.header.status) {
    case http::StatusCode::OK:
        if (!res.header.ContentType) {
            SPDLOG_TRACE("missing content-type header for {}", req.Url().url);
            return;
        }
        if (res.header.ContentType->value.starts_with("text/html")) {
            ProcessHTMLDocument(req, res);
        } else {
            spdlog::debug("unsupported content-type {} for {}", res.header.ContentType->value, req.Url().url);
        }
        break;

    case http::StatusCode::MovedPermanently:
    case http::StatusCode::Found:
    case http::StatusCode::SeeOther:
    case http::StatusCode::TemporaryRedirect:
    case http::StatusCode::PermanentRedirect:
        {
            if (!res.header.Location) {
                return;
            }

            std::string location{res.header.Location->value};
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
        spdlog::info("unhandled status {} for {}", res.header.status, req.Url().url);
        break;
    }
}

std::pair<data::docid_t, std::string> Worker::NextDocument() {
    using namespace std::string_view_literals;
    data::docid_t docID = state_.nextDocumentID.fetch_add(1);
    auto chunk = docID / DocumentChunkSize;

    auto chunkStr = NumberedEntity("chunk"sv, chunk, 10);
    auto docStr = NumberedEntity("doc"sv, docID, 10);

    auto chunkPath = docsDirectory_ + "/" + chunkStr;
    if (!lastChunk_.HasValue() || chunk != *lastChunk_) {
        lastChunk_ = {chunk};
        if (mkdir(chunkPath.c_str(), 0755) != 0) {
            if (errno != EEXIST) {
                spdlog::error("failed to create chunk {}: {}", chunkPath, strerror(errno));
                return {docID, ""};
            }
        }
    }

    auto docPath = chunkPath + "/" + docStr;
    return {docID, std::move(docPath)};
}

}  // namespace mithril
