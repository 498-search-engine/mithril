#ifndef CRAWLER_COORDINATOR_H
#define CRAWLER_COORDINATOR_H

#include "Config.h"
#include "DocumentQueue.h"
#include "HostRateLimiter.h"
#include "RequestManager.h"
#include "State.h"
#include "StringTrie.h"
#include "UrlFrontier.h"
#include "core/memory.h"
#include "metrics/MetricsServer.h"

#include <cstddef>
#include <string>

namespace mithril {

class Coordinator {
public:
    explicit Coordinator(const CrawlerConfig& config);
    void Run();

private:
    void SnapshotThreadEntry(size_t n);
    void DoSnapshot(size_t n);

    std::string LockPath() const;
    std::string StatePath() const;

    void DumpState(const std::string& file);
    void RecoverState(const std::string& file);

    const CrawlerConfig config_;
    StringTrie blacklistedHostsTrie_;

    std::string frontierDirectory_;

    core::UniquePtr<HostRateLimiter> limiter_;
    core::UniquePtr<LiveState> state_;

    core::UniquePtr<DocumentQueue> docQueue_;
    core::UniquePtr<UrlFrontier> frontier_;
    core::UniquePtr<RequestManager> requestManager_;
    core::UniquePtr<metrics::MetricsServer> metricsServer_;
};

}  // namespace mithril

#endif
