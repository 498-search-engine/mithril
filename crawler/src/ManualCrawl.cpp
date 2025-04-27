#include "Worker.h"
#include "core/memory.h"
#include "data/Document.h"
#include "html/Parser.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Resolver.h"
#include "http/SSL.h"
#include "http/SyncResolver.h"
#include "http/URL.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include "core/pair.h"

using namespace mithril;

namespace {

int ManualCrawl(const std::string& url, data::docid_t docID, const std::string& outputPath) {
    auto parsed = http::ParseURL(url);
    if (!parsed) {
        spdlog::error("failed to parse url");
        return 1;
    }

    http::RequestExecutor executor;
    executor.Add(mithril::http::Request::GET(std::move(*parsed),
                                             http::RequestOptions{
                                                 .followRedirects = 5,
                                                 .timeout = 10,
                                                 .enableCompression = true,
                                             }));

    spdlog::info("starting request");
    while (executor.InFlightRequests() > 0) {
        executor.ProcessConnections();
    }

    if (executor.ReadyResponses().empty()) {
        auto failed = std::move(executor.FailedRequests().front());
        executor.FailedRequests().pop_back();
        spdlog::error("request failed: {}", http::StringOfRequestError(failed.error));
        return 1;
    }

    assert(!executor.ReadyResponses().empty());
    auto res = std::move(executor.ReadyResponses().front());
    executor.ReadyResponses().pop_back();

    try {
        res.res.DecodeBody();
    } catch (const std::exception& err) {
        spdlog::error("failed to decode response: {}", err.what());
        return 1;
    }

    if (res.res.header.status != 200) {
        spdlog::warn("got status code {}", res.res.header.status);
    }

    auto body = std::string_view{res.res.body.data(), res.res.body.size()};

    spdlog::info("parsing document");
    html::ParsedDocument doc;
    html::ParseDocument(body, doc);

    auto description = GetDescription(doc);

    WriteDocumentToFile(outputPath,
                        data::DocumentView{
                            .id = docID,
                            .url = res.req.Url().url,
                            .title = doc.titleWords,
                            .description = description,
                            .words = doc.words,
                            .forwardLinks = {},  // empty
                        });

    spdlog::info("wrote document to {}", outputPath);
    return 0;
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 4) {
        spdlog::error("usage: {} <url> <doc_id> <output_path>", argv[0]);
        return 1;
    }


    auto url = std::string{argv[1]};
    auto docID = static_cast<data::docid_t>(std::stoul(std::string{argv[2]}));
    auto outputPath = std::string{argv[3]};

    http::InitializeSSL();
    http::ApplicationResolver = core::UniquePtr<mithril::http::Resolver>(new mithril::http::SyncResolver{});

    auto res = ManualCrawl(url, docID, outputPath);

    http::ApplicationResolver.Reset(nullptr);
    http::DeinitializeSSL();

    return res;
}
