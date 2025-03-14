#include "Coordinator.h"

#include "Config.h"
#include "CrawlerMetrics.h"
#include "DocumentQueue.h"
#include "FileSystem.h"
#include "RequestManager.h"
#include "State.h"
#include "UrlFrontier.h"
#include "Worker.h"
#include "core/memory.h"
#include "core/thread.h"
#include "data/Deserialize.h"
#include "data/Reader.h"
#include "data/Serialize.h"
#include "data/Writer.h"
#include "metrics/MetricsServer.h"

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <sys/signal.h>
#include <sys/stat.h>

namespace mithril {

constexpr size_t NumWorkers = 2;
constexpr size_t ConcurrentRequests = 10;

Coordinator::Coordinator(const CrawlerConfig& config) : config_(config) {
    if (!DirectoryExists(config.data_directory.c_str())) {
        spdlog::error("configured data_directory does not exist: {}", config.data_directory);
        exit(1);
    }

    frontierDirectory_ = config.data_directory + "/frontier";
    if (!DirectoryExists(frontierDirectory_.c_str())) {
        // Create frontier directory
        mkdir(frontierDirectory_.c_str(), 0755);
    }

    docsDirectory_ = config.data_directory + "/docs";
    if (!DirectoryExists(docsDirectory_.c_str())) {
        // Create docs directory
        mkdir(docsDirectory_.c_str(), 0755);
    }

    state_ = core::UniquePtr<LiveState>(new LiveState{});

    docQueue_ = core::UniquePtr<DocumentQueue>(new DocumentQueue{state_->threadSync});
    frontier_ = core::UniquePtr<UrlFrontier>(new UrlFrontier{frontierDirectory_, config.concurrent_robots_requests});
    requestManager_ = core::UniquePtr<RequestManager>(new RequestManager{frontier_.Get(), docQueue_.Get(), config});

    metricsServer_ = core::UniquePtr<metrics::MetricsServer>(new metrics::MetricsServer{config.metrics_port});
    RegisterCrawlerMetrics(*metricsServer_);

    RecoverState();
}

void Coordinator::Run() {
    if (frontier_->TotalSize() == 0) {
        spdlog::info("frontier is fresh - seeding with {} seed URLs", config_.seed_urls.size());
        // Add all seed URLs
        for (const auto& url : config_.seed_urls) {
            frontier_->PushURL(url);
        }
    } else {
        spdlog::info("resuming crawl with {} documents in frontier", frontier_->TotalSize());
    }

    if (frontier_->Empty()) {
        spdlog::warn("no pending urls in frontier, exiting");
        return;
    }

    size_t threadCount = 0;
    std::vector<core::Thread> workerThreads;
    workerThreads.reserve(config_.num_workers);

    for (size_t i = 0; i < config_.num_workers; ++i) {
        workerThreads.emplace_back([&] {
            Worker w(*state_, docQueue_.Get(), frontier_.Get(), docsDirectory_);
            w.Run();
        });
        ++threadCount;
    }

    core::Thread requestThread([&] { requestManager_->Run(state_->threadSync); });
    ++threadCount;
    core::Thread robotsThread([&] { frontier_->RobotsRequestsThread(state_->threadSync); });
    ++threadCount;
    core::Thread freshURLsThread([&] { frontier_->FreshURLsThread(state_->threadSync); });
    ++threadCount;
    core::Thread metricsThread([&] { metricsServer_->Run(state_->threadSync); });
    ++threadCount;

    // Wait for SIGINT or SIGTERM
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    int sig = 0;
    while (sig != SIGINT && sig != SIGTERM) {
        sigwait(&signals, &sig);
    }

    spdlog::info("received signal {} {}, shutting down", sig, strsignal(sig));

    // Send shutdown to threads
    state_->threadSync.Shutdown();

    // std::atomic<bool> keepStopping{true};

    // core::Thread shutdownThread([&] {
    //     while (keepStopping.load()) {
    //         spdlog::info("sending shutdown");
    //         state_->threadSync.Shutdown();
    //         usleep(10 * 1000);
    //     }
    // });

    // Wait for threads to finish
    requestThread.Join();
    robotsThread.Join();
    freshURLsThread.Join();
    for (auto& t : workerThreads) {
        t.Join();
    }
    metricsThread.Join();

    // keepStopping.store(false);
    // shutdownThread.Join();

    spdlog::info("all threads stopped, saving crawler state");
    DumpState();
    spdlog::info("shutdown complete, goodbye!");
}

std::string Coordinator::StatePath() const {
    return config_.data_directory + "/state.dat";
}

void Coordinator::DumpState() {
    auto stateFilePath = StatePath();
    auto stateFileTempPath = stateFilePath + ".tmp";

    PersistentState state;
    state.nextDocumentID = state_->nextDocumentID.load();
    frontier_->DumpPendingURLs(state.pendingURLs);
    requestManager_->ExtractQueuedURLs(state.activeCrawlURLs);
    docQueue_->ExtractCompletedURLs(state.activeCrawlURLs);

    spdlog::debug("saved state: next document id = {}", state.nextDocumentID);
    spdlog::debug("saved state: pending url count = {}", state.pendingURLs.size());
    spdlog::debug("saved state: active crawl url count = {}", state.activeCrawlURLs.size());

    {
        // Write state to file
        // TODO: Should we gzip the crawler state?
        auto f = data::FileWriter{stateFileTempPath.c_str()};
        data::SerializeValue(state, f);
    }

    // Replace any old state file
    int status = rename(stateFileTempPath.c_str(), stateFilePath.c_str());
    if (status == -1) {
        spdlog::error("failed to dump crawler state to disk: {}", std::strerror(errno));
    }
}

void Coordinator::RecoverState() {
    auto stateFilePath = StatePath();
    if (!FileExists(stateFilePath.c_str())) {
        spdlog::info("no state file found at {}", stateFilePath);
        return;
    }

    PersistentState state;
    {
        auto f = data::FileReader{stateFilePath.c_str()};
        data::DeserializeValue(state, f);
    }

    spdlog::debug("loaded state: next document id = {}", state.nextDocumentID);
    spdlog::debug("loaded state: pending url count = {}", state.pendingURLs.size());
    spdlog::debug("loaded state: active crawl url count = {}", state.activeCrawlURLs.size());

    state_->nextDocumentID.store(state.nextDocumentID);
    frontier_->PushURLs(state.pendingURLs);
    requestManager_->RestoreQueuedURLs(state.activeCrawlURLs);
}

}  // namespace mithril
