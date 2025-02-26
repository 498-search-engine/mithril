#ifndef CRAWLER_MIDDLEQUEUE_H
#define CRAWLER_MIDDLEQUEUE_H


#include "ThreadSync.h"
#include "UrlFrontier.h"
#include "core/memory.h"
#include "core/optional.h"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mithril {

class MiddleQueue {
public:
    MiddleQueue(UrlFrontier* frontier, size_t numQueues);

    void GetURLs(ThreadSync& sync, size_t max, std::vector<std::string>& out, bool atLeastOne = false);

    void RestoreFrom(std::vector<std::string>& urls);
    void ExtractQueuedURLs(std::vector<std::string>& out);

private:
    struct HostRecord {
        std::string host;
        long crawlDelayMs{};
        long earliestNextCrawl{};
        std::queue<std::string> queue;
        core::Optional<size_t> activeQueue;
    };

    size_t ActiveQueueCount() const;
    double QueueUtilization() const;

    void AcceptURL(long now, std::string url);
    void PushURLForHost(std::string url, HostRecord* record);
    void PushURLForNewHost(long now, std::string url, std::string host);

    std::string PopFromHost(long now, HostRecord& record);

    void PopulateActiveQueues();
    void AssignFreeQueue(HostRecord* record);
    void CleanEmptyHosts();

    bool WantURL(std::string_view url) const;

    UrlFrontier* frontier_;
    size_t n_;
    size_t k_{0};

    std::unordered_map<std::string, core::UniquePtr<HostRecord>> hosts_;
    std::vector<HostRecord*> queues_;
    std::vector<size_t> emptyQueues_;
    size_t totalQueuedURLs_{0};
};

}  // namespace mithril

#endif
