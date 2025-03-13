#include "RequestManager.h"

#include "CrawlerMetrics.h"
#include "DocumentQueue.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/URL.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

RequestManager::RequestManager(UrlFrontier* frontier, DocumentQueue* docQueue, const CrawlerConfig& config)
    : targetConcurrentReqs_(config.concurrent_requests),
      requestTimeout_(config.request_timeout),
      middleQueue_(frontier, config),
      docQueue_(docQueue) {}

void RequestManager::Run(ThreadSync& sync) {
    std::vector<std::string> urls;
    urls.reserve(targetConcurrentReqs_);

    while (!sync.ShouldShutdown()) {
        sync.MaybePause();
        InFlightCrawlRequestsMetric.Set(requestExecutor_.InFlightRequests());
        // Get more URLs to execute, up to targetSize_ concurrently executing
        if (requestExecutor_.InFlightRequests() < targetConcurrentReqs_) {
            // If we have no in-flight requests to process, wait for at least
            // one URL to become available from the frontier.
            bool wantAtLeastOne = requestExecutor_.InFlightRequests() == 0;

            size_t toAdd = targetConcurrentReqs_ - requestExecutor_.InFlightRequests();
            urls.clear();
            middleQueue_.GetURLs(sync, toAdd, urls, wantAtLeastOne);
            if (sync.ShouldSynchronize()) {
                continue;
            }

            if (urls.empty() && requestExecutor_.InFlightRequests() == 0) {
                continue;
            }

            for (const auto& url : urls) {
                if (auto parsed = http::ParseURL(url)) {
                    SPDLOG_DEBUG("starting crawl request: {}", url);
                    requestExecutor_.Add(
                        http::Request::GET(std::move(*parsed),
                                           http::RequestOptions{
                                               .timeout = 30, // seconds
                                               .maxResponseSize = 2 * 1024 * 1024, // 2 MB
                                               .allowedContentLanguage = {"en", "en-*", "en_*"}, // English
                    }));
                } else {
                    spdlog::info("frontier failed to parse url {}", url);
                }
            }
        }

        if (requestExecutor_.InFlightRequests() == 0) {
            continue;
        }

        // Process send/recv for connections
        requestExecutor_.ProcessConnections();

        // Process ready responses
        auto& ready = requestExecutor_.ReadyResponses();
        if (!ready.empty()) {
            docQueue_->PushAll(ready);
            ready.clear();
        }

        // Process requests that failed
        auto& failed = requestExecutor_.FailedRequests();
        if (!failed.empty()) {
            for (auto& f : failed) {
                DispatchFailedRequest(std::move(f));
            }
            failed.clear();
        }
    }

    spdlog::info("request manager terminating");
}

void RequestManager::DispatchFailedRequest(http::FailedRequest failed) {
    spdlog::warn("failed crawl request: {} {}", failed.req.Url().url, http::StringOfRequestError(failed.error));
    // TODO: pass off to whatever
}

void RequestManager::RestoreQueuedURLs(std::vector<std::string>& urls) {
    middleQueue_.RestoreFrom(urls);
}

void RequestManager::ExtractQueuedURLs(std::vector<std::string>& out) {
    middleQueue_.ExtractQueuedURLs(out);
    requestExecutor_.DumpUnprocessedRequests(out);
}

}  // namespace mithril
