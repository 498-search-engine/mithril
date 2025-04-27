#include "RequestManager.h"

#include "Config.h"
#include "CrawlerMetrics.h"
#include "DocumentQueue.h"
#include "Globals.h"
#include "HostRateLimiter.h"
#include "StringTrie.h"
#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "Util.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include "core/pair.h"
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

RequestManager::RequestManager(UrlFrontier* frontier,
                               HostRateLimiter* limiter,
                               DocumentQueue* docQueue,
                               const CrawlerConfig& config,
                               const StringTrie& blacklistedHosts)
    : targetConcurrentReqs_(config.concurrent_requests),
      requestTimeout_(config.request_timeout),
      middleQueue_(frontier, limiter, config),
      limiter_(limiter),
      docQueue_(docQueue),
      blacklistedHosts_(blacklistedHosts) {}

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
                    auto hostParts = SplitString(parsed->host, '.');
                    std::reverse(hostParts.begin(), hostParts.end());
                    if (blacklistedHosts_.ContainsPrefix(hostParts)) {
                        SPDLOG_TRACE("url {} is from blacklisted host", url);
                        continue;
                    }

                    SPDLOG_DEBUG("starting crawl request: {}", url);
                    requestExecutor_.Add(http::Request::GET(std::move(*parsed),
                                                            http::RequestOptions{
                                                                .timeout = static_cast<int>(requestTimeout_),
                                                                .maxResponseSize = MaxResponseSize,
                                                                .allowedMimeType = AllowedMimeTypes,
                                                                .allowedContentLanguage = AllowedLanguages,
                                                                .enableCompression = true,
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
            for (const auto& f : failed) {
                DispatchFailedRequest(f);
            }
            failed.clear();
        }
    }

    spdlog::info("request manager terminating");
}

void RequestManager::TouchRequestTimeouts() {
    requestExecutor_.TouchRequestTimeouts();
}

void RequestManager::DispatchFailedRequest(const http::FailedRequest& failed) {
    auto s = std::string{http::StringOfRequestError(failed.error)};
    spdlog::warn("failed crawl request: {} {}", failed.req.Url().url, s);

    CrawlRequestErrorsMetric
        .WithLabels({
            {"error", s}
    })
        .Inc();
}

void RequestManager::RestoreQueuedURLs(std::vector<std::string>& urls) {
    middleQueue_.RestoreFrom(urls);
}

void RequestManager::DumpQueuedURLs(std::vector<std::string>& out) {
    middleQueue_.DumpQueuedURLs(out);
    requestExecutor_.DumpUnprocessedRequests(out);
}

}  // namespace mithril
