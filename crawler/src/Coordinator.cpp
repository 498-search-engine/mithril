#include "Coordinator.h"

#include "DocumentQueue.h"
#include "RequestManager.h"
#include "UrlFrontier.h"
#include "Worker.h"

#include <memory>
#include <thread>

namespace mithril {

constexpr size_t NumWorkers = 2;
constexpr size_t ConcurrentRequests = 10;

Coordinator::Coordinator() {
    frontier_ = std::make_unique<UrlFrontier>();
    docQueue_ = std::make_unique<DocumentQueue>();
    requestManager_ = std::make_unique<RequestManager>(ConcurrentRequests, frontier_.get(), docQueue_.get());
}

void Coordinator::Run() {
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Kennett_Square,_Pennsylvania");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Pennsylvania");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/California");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Florida");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Maine");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Texas");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Ohio");
    // frontier_->PutURL("https://en.wikipedia.org/wiki/Nebraska");
    frontier_->PutURL("https://dnsge.org");

    std::vector<std::thread> workerThreads;
    workerThreads.reserve(NumWorkers);
    for (size_t i = 0; i < NumWorkers; ++i) {
        workerThreads.emplace_back([docQueue = docQueue_.get(), frontier = frontier_.get()] {
            Worker w(docQueue, frontier);
            w.Run();
        });
    }

    std::thread eThread([r = requestManager_.get()] { r->Run(); });
    eThread.join();

    docQueue_->Close();

    for (auto& t : workerThreads) {
        t.join();
    }
}

}  // namespace mithril
