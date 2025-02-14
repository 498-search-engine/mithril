#include "RequestManager.h"

#include "DocumentQueue.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/URL.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace mithril {

RequestManager::RequestManager(size_t targetConcurrentReqs, UrlFrontier* frontier, DocumentQueue* docQueue)
    : targetConcurrentReqs_(targetConcurrentReqs), frontier_(frontier), docQueue_(docQueue) {}

void RequestManager::Run() {
    std::vector<std::string> urls;
    urls.reserve(targetConcurrentReqs_);

    while (!stopped_.load(std::memory_order_acquire)) {
        // Get more URLs to execute, up to targetSize_ concurrently executing
        if (requestExecutor_.InFlightRequests() < targetConcurrentReqs_) {
            // If we have no in-flight requests to process, wait for at least
            // one URL to become available from the frontier.
            bool wantAtLeastOne = requestExecutor_.InFlightRequests() == 0;

            size_t toAdd = targetConcurrentReqs_ - requestExecutor_.InFlightRequests();
            urls.clear();
            frontier_->GetURLs(toAdd, urls, wantAtLeastOne);

            for (const auto& url : urls) {
                if (auto parsed = http::ParseURL(url)) {
                    requestExecutor_.Add(http::Request::GET(std::move(*parsed)));
                } else {
                    std::cerr << "bad url: " << url << std::endl;
                }
            }
        }

        // We should have at least one request to execute
        assert(requestExecutor_.InFlightRequests() > 0);

        // Process send/recv for connections
        requestExecutor_.ProcessConnections();

        // Process ready responses
        auto& ready = requestExecutor_.ReadyResponses();
        if (!ready.empty()) {
            for (auto& r : ready) {
                DispatchReadyResponse(std::move(r));
            }
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

    std::cout << "request manager terminating" << std::endl;
}

void RequestManager::Stop() {
    stopped_.store(true, std::memory_order_release);
}

void RequestManager::DispatchReadyResponse(http::CompleteResponse res) {
    docQueue_->Push(std::move(res));
}

void RequestManager::DispatchFailedRequest(http::FailedRequest failed) {
    std::cout << "failed " << failed.req.Url().url << ": " << http::StringOfRequestError(failed.error) << std::endl;
    // TODO: pass off to whatever
}

}  // namespace mithril
