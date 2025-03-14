#include "Coordinator.h"

#include "Clock.h"
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

    frontier_->InitSync(state_->threadSync);
    RegisterCrawlerMetrics(*metricsServer_);

    RecoverState(StatePath());
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

    core::Thread snapshotThread([this, threadCount] { this->SnapshotThreadEntry(threadCount); });

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

    // Wait for threads to finish
    requestThread.Join();
    robotsThread.Join();
    freshURLsThread.Join();
    for (auto& t : workerThreads) {
        t.Join();
    }
    snapshotThread.Join();
    metricsThread.Join();

    spdlog::info("all threads stopped, saving crawler state");
    DumpState(StatePath());
    spdlog::info("shutdown complete, goodbye!");
}

std::string Coordinator::StatePath() const {
    return config_.data_directory + "/state.dat";
}

void Coordinator::DumpState(const std::string& file) {
    auto stateFileTempPath = file + ".tmp";

    PersistentState state;
    state.nextDocumentID = state_->nextDocumentID.load();
    frontier_->DumpPendingURLs(state.pendingURLs);
    requestManager_->DumpQueuedURLs(state.activeCrawlURLs);
    docQueue_->DumpCompletedURLs(state.activeCrawlURLs);

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
    int status = rename(stateFileTempPath.c_str(), file.c_str());
    if (status == -1) {
        spdlog::error("failed to dump crawler state to disk: {}", std::strerror(errno));
    }
}

void Coordinator::RecoverState(const std::string& file) {
    if (!FileExists(file.c_str())) {
        spdlog::info("no state file found at {}", file);
        return;
    }

    PersistentState state;
    {
        auto f = data::FileReader{file.c_str()};
        data::DeserializeValue(state, f);
    }

    spdlog::debug("loaded state: next document id = {}", state.nextDocumentID);
    spdlog::debug("loaded state: pending url count = {}", state.pendingURLs.size());
    spdlog::debug("loaded state: active crawl url count = {}", state.activeCrawlURLs.size());

    state_->nextDocumentID.store(state.nextDocumentID);
    frontier_->PushURLs(state.pendingURLs);
    requestManager_->RestoreQueuedURLs(state.activeCrawlURLs);
}

void Coordinator::SnapshotThreadEntry(size_t n) {
    auto start = MonotonicTime();
    while (!state_->threadSync.ShouldShutdown()) {
        sleep(1);
        if (state_->threadSync.ShouldShutdown()) {
            return;
        }

        auto now = MonotonicTime();
        if (now - start >= config_.snapshot_period_seconds) {
            DoSnapshot(n);
            start = MonotonicTime();
        }
    }
}

void Coordinator::DoSnapshot(size_t n) {
    spdlog::info("requesting pause for snapshot");
    state_->threadSync.StartPause(static_cast<int>(n));
    spdlog::info("taking snapshot of crawler state");

    auto snapshotDir = config_.data_directory + "/snapshot";
    auto snapshotTempDir = config_.data_directory + "/snapshot.tmp";
    auto snapshotOldDir = config_.data_directory + "/snapshot.old";

    if (DirectoryExists(snapshotTempDir.c_str())) {
        RmRf(snapshotTempDir.c_str());
    }
    mkdir(snapshotTempDir.c_str(), 0755);

    DumpState(snapshotTempDir + "/state.dat");
    bool ok = frontier_->CopyStateToDirectory(snapshotTempDir);
    if (!ok) {
        spdlog::error("failed to copy frontier state to snapshot directory");
        state_->threadSync.EndPause();
        return;
    }

    bool cleanOld = false;
    if (DirectoryExists(snapshotDir.c_str())) {
        if (DirectoryExists(snapshotOldDir.c_str())) {
            RmRf(snapshotOldDir.c_str());
        }

        int status = rename(snapshotDir.c_str(), snapshotOldDir.c_str());
        if (status == -1) {
            spdlog::error("failed to rename old snapshot: {}", std::strerror(errno));
            state_->threadSync.EndPause();
            return;
        }
        cleanOld = true;
    }

    int status = rename(snapshotTempDir.c_str(), snapshotDir.c_str());
    if (status == -1) {
        spdlog::error("failed to copy frontier state to snapshot directory: {}", std::strerror(errno));
        state_->threadSync.EndPause();
        return;
    }

    if (cleanOld) {
        RmRf(snapshotOldDir.c_str());
    }

    spdlog::info("resuming crawler");
    state_->threadSync.EndPause();
}

std::string Coordinator::StateSnapshotPath() const {
    return config_.data_directory + "/state.dat";
}

}  // namespace mithril
