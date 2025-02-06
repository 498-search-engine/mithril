#ifndef CRAWLER_COORDINATOR_H
#define CRAWLER_COORDINATOR_H

#include "DocumentQueue.h"
#include "RequestManager.h"
#include "UrlFrontier.h"

#include <memory>

namespace mithril {

class Coordinator {
public:
    Coordinator();

    void Run();

private:
    std::unique_ptr<UrlFrontier> frontier_;
    std::unique_ptr<RequestManager> requestManager_;
    std::unique_ptr<DocumentQueue> docQueue_;
};

}  // namespace mithril

#endif
