#ifndef CRAWLER_COORDINATOR_H
#define CRAWLER_COORDINATOR_H

#include "DocumentQueue.h"
#include "RequestManager.h"
#include "UrlFrontier.h"
#include "config.h"

#include <memory>

namespace mithril {

class Coordinator {
public:
    explicit Coordinator(const CrawlerConfig& config);
    void Run();
    
private:
    const CrawlerConfig config_;
    std::unique_ptr<UrlFrontier> frontier_;
    std::unique_ptr<RequestManager> requestManager_;
    std::unique_ptr<DocumentQueue> docQueue_;
};

}  // namespace mithril

#endif
