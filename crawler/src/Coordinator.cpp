#include "Coordinator.h"

#include "DocumentQueue.h"
#include "RequestManager.h"
#include "UrlFrontier.h"
#include "Worker.h"

#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace mithril {

constexpr size_t NumWorkers = 2;
constexpr size_t ConcurrentRequests = 10;

Coordinator::Coordinator(const CrawlerConfig& config) : config_(config) {
    frontier_ = std::make_unique<UrlFrontier>();
    docQueue_ = std::make_unique<DocumentQueue>();
    requestManager_ = std::make_unique<RequestManager>(
        config_.concurrent_requests, config_.request_timeout, frontier_.get(), docQueue_.get());
}

void Coordinator::Run() {
    // Add all seed URLs
    for (const auto& url : config_.seed_urls) {
        frontier_->PutURL(url);
    }

    std::vector<std::thread> workerThreads;
    workerThreads.reserve(config_.num_workers);

    for (size_t i = 0; i < config_.num_workers; ++i) {
        workerThreads.emplace_back([docQueue = docQueue_.get(), frontier = frontier_.get()] {
            Worker w(docQueue, frontier);
            w.Run();
        });
    }

    std::thread requestThread([r = requestManager_.get()] { r->Run(); });

    // TODO: shutdown strategy for threads
    std::thread frontierThread([f = frontier_.get()] {
        while (true) {
            f->ProcessRobotsRequests();
        }
    });

    requestThread.join();
    frontierThread.join();

    docQueue_->Close();

    for (auto& t : workerThreads) {
        t.join();
    }
}
}  // namespace mithril
