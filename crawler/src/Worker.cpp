#include "Worker.h"

#include "Clock.h"
#include "CrawlerMetrics.h"
#include "DocumentQueue.h"
#include "Globals.h"
#include "State.h"
#include "StringTrie.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "Util.h"
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

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <sys/stat.h>

namespace mithril {

using namespace std::string_view_literals;

namespace {

const std::set<std::string_view> BlacklistedBasePaths = {
    "/search"sv,
};

void WriteDocumentToFile(const std::string& fileName, const data::DocumentView& doc) {
    try {
        auto fWriter = data::FileWriter{fileName.c_str()};
        auto zipWriter = data::GzipWriter{fWriter};
        data::SerializeValue(doc, zipWriter);
        zipWriter.Finish();
        fWriter.DontNeed();
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

std::vector<std::string_view> GetDescription(const html::ParsedDocument& doc) {
    auto descIt = doc.metas.find("description"sv);
    if (descIt == doc.metas.end()) {
        return {};
    }

    return GetWords(descIt->second);
}

struct RobotsMeta {
    bool NoIndex{false};
    bool NoFollow{false};
};

RobotsMeta GetRobotsMeta(const html::ParsedDocument& doc) {
    auto res = RobotsMeta{};

    auto robotsIt = doc.metas.find("robots"sv);
    if (robotsIt == doc.metas.end()) {
        return res;
    }

    auto rules = GetCommaSeparatedList(robotsIt->second);
    for (auto rule : rules) {
        if (rule == "noindex"sv) {
            res.NoIndex = true;
        } else if (rule == "nofollow"sv) {
            res.NoFollow = true;
        }
    }

    return res;
}

}  // namespace

Worker::Worker(LiveState& state,
               DocumentQueue* docQueue,
               UrlFrontier* frontier,
               std::string docsDirectory,
               const StringTrie& blacklistedHosts)
    : state_(state),
      docQueue_(docQueue),
      frontier_(frontier),
      docsDirectory_(std::move(docsDirectory)),
      blacklistedHosts_(blacklistedHosts) {}

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
    bool indexDocument = true;
    bool followLinks = true;

    // Parse HTML document from response body
    spdlog::info("processing document {}", req.Url().url);
    html::ParseDocument(std::string_view{res.body.data(), res.body.size()}, parsedDoc_);

    if (parsedDoc_.titleWords.empty() || parsedDoc_.words.empty()) {
        // Not worth indexing
        spdlog::info("discarding {} due to empty title/words", req.Url().url);
        return;
    }

    if (!parsedDoc_.lang.empty()) {
        // Check if valid language
        auto anyMatch = std::any_of(
            AllowedLanguages.begin(), AllowedLanguages.end(), [htmlLang = parsedDoc_.lang](std::string_view lang) {
                return http::ContentLanguageMatches(htmlLang, lang);
            });
        if (!anyMatch) {
            spdlog::info("discarding {} due to lang {}", req.Url().url, parsedDoc_.lang);
            return;
        }
    }

    // Check if <meta name="robots"> tag exists with rules
    auto robotsMeta = GetRobotsMeta(parsedDoc_);
    if (robotsMeta.NoIndex) {
        indexDocument = false;
    }
    if (robotsMeta.NoFollow) {
        followLinks = false;
    }

    auto followURLs = std::vector<std::string>{};
    if (followLinks) {
        followURLs = GetFollowURLs(parsedDoc_, req.Url());
    }

    if (indexDocument) {
        // Extract description from <meta name="description"> tag if present
        auto description = GetDescription(parsedDoc_);

        // Save document to crawl corpus
        SaveDocument(data::DocumentView{
            .url = req.Url().url,
            .title = parsedDoc_.titleWords,
            .description = description,
            .words = parsedDoc_.words,
            .forwardLinks = followURLs,
        });

        DocumentSizeBytesMetric.Observe(res.body.size());
    }

    if (followLinks && !followURLs.empty()) {
        // Push links found on page into frontier
        spdlog::debug("pushing {} urls to frontier from {}", followURLs.size(), req.Url().url);
        frontier_->PushURLs(followURLs);
    }
}

void Worker::SaveDocument(data::DocumentView doc) {
    auto [docID, docPath] = NextDocument();
    if (docPath.empty()) {
        spdlog::error("failed to get next document path");
        return;
    }

    doc.id = docID;
    WriteDocumentToFile(docPath, doc);
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

std::vector<std::string> Worker::GetFollowURLs(const html::ParsedDocument& doc, const http::URL& url) const {
    std::vector<std::string> absoluteURLs;
    absoluteURLs.reserve(doc.links.size());
    for (const auto& l : doc.links) {
        // Handle relative URLs
        auto absoluteLink = html::MakeAbsoluteLink(url, doc.base, l.url);
        if (!absoluteLink) {
            continue;
        }

        auto parsed = http::ParseURL(*absoluteLink);
        if (!parsed) {
            continue;
        }

        // Canonicalize URL
        auto canonical = http::CanonicalizeURL(*parsed);

        // Check whether base path is blacklisted
        if (BlacklistedBasePaths.contains(canonical.BasePath())) {
            SPDLOG_TRACE("url {} has blacklisted base path", canonical.url);
            continue;
        }

        // Check whether host is blacklisted
        auto hostParts = SplitString(canonical.host, '.');
        std::reverse(hostParts.begin(), hostParts.end());
        if (blacklistedHosts_.ContainsPrefix(hostParts)) {
            SPDLOG_TRACE("url {} is from blacklisted host", canonical.url);
            continue;
        }

        absoluteURLs.push_back(std::move(canonical.url));
    }
    return absoluteURLs;
}

}  // namespace mithril
