#include "Coordinator.h"

#include "Config.h"
#include "DocumentQueue.h"
#include "FileSystem.h"
#include "RequestManager.h"
#include "UrlFrontier.h"
#include "Worker.h"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

constexpr size_t NumWorkers = 2;
constexpr size_t ConcurrentRequests = 10;

Coordinator::Coordinator(const CrawlerConfig& config) : config_(config) {
    if (!DirectoryExists(config.frontierDirectory.c_str())) {
        spdlog::error("configured frontier_directory does not exist: {}", config.frontierDirectory);
        exit(1);
    }

    docQueue_ = std::make_unique<DocumentQueue>();
    requestManager_ = std::make_unique<RequestManager>(
        config_.concurrent_requests, config_.request_timeout, frontier_.get(), docQueue_.get());
}

void Coordinator::Run() {
    // Add all seed URLs
    for (const auto& url : config_.seed_urls) {
        frontier_->PushURL(url);
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
    std::thread robotsThread([f = frontier_.get()] {
        while (true) {
            f->ProcessRobotsRequests();
        }
    });
    std::thread freshURLsThread([f = frontier_.get()] {
        while (true) {
            f->ProcessFreshURLs();
        }
    });

    requestThread.join();
    robotsThread.join();
    freshURLsThread.join();

    docQueue_->Close();

    for (auto& t : workerThreads) {
        t.join();
    }
}
}  // namespace mithril
