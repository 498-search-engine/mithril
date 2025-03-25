#ifndef COMMON_HTTP_ASYNCRESOLVER_H
#define COMMON_HTTP_ASYNCRESOLVER_H

#include "core/cv.h"
#include "core/lru_cache.h"
#include "core/mutex.h"
#include "core/thread.h"
#include "http/Resolver.h"

#include <cstddef>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace mithril::http {

class AsyncResolver : public Resolver {

public:
    AsyncResolver(size_t cacheSize);
    AsyncResolver(size_t workers, size_t cacheSize);
    ~AsyncResolver();

    bool Resolve(const std::string& host, const std::string& port, Resolver::ResolutionResult& result) override;

private:
    struct ResolveRequest {
        std::string host;
        std::string port;
        std::string key;
    };

    void StartResolve(const std::string& host, const std::string& port, const std::string& key);

    void WorkerThreadEntry();
    void ResolveSync(const ResolveRequest& req);

    mutable core::Mutex resultsMu_;
    mutable core::Mutex activeMu_;
    core::cv requestsCv_;
    std::vector<core::Thread> workers_;

    bool shutdown_{false};

    core::LRUCache<std::string, std::optional<Resolver::ResolutionResult>> results_;
    std::queue<ResolveRequest> activeRequests_;
};

}  // namespace mithril::http

#endif
