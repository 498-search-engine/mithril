#ifndef CRAWLER_COORDINATOR_H
#define CRAWLER_COORDINATOR_H

#include "Config.h"
#include "DocumentQueue.h"
#include "RequestManager.h"
#include "State.h"
#include "UrlFrontier.h"
#include "core/memory.h"

#include <string>

namespace mithril {

class Coordinator {
public:
    explicit Coordinator(const CrawlerConfig& config);
    void Run();

private:
    std::string StatePath() const;

    void DumpState();
    void RecoverState();

    const CrawlerConfig config_;

    std::string frontierDirectory_;
    std::string docsDirectory_;

    core::UniquePtr<LiveState> state_;

    core::UniquePtr<DocumentQueue> docQueue_;
    core::UniquePtr<UrlFrontier> frontier_;
    core::UniquePtr<RequestManager> requestManager_;
};

}  // namespace mithril

#endif
