#ifndef COMMON_HTTP_ASYNCRESOLVER_H
#define COMMON_HTTP_ASYNCRESOLVER_H

#include "core/locks.h"
#include "core/mutex.h"
#include "core/thread.h"
#include "http/Resolver.h"

#include <atomic>
#include <list>
#include <netdb.h>
#include <optional>
#include <string>
#include <unordered_map>

namespace mithril::http {

class AsyncResolver : public Resolver {
public:
    AsyncResolver();
    ~AsyncResolver();

    bool Resolve(const std::string& host, const std::string& port, Resolver::ResolutionResult& result) override;

private:
    struct ResolveRequest {
        gaicb request;
        addrinfo hints;
        std::string host;
        std::string port;
        std::string key;
    };

    void StartResolve(const std::string& host, const std::string& port, const std::string& key);

    void WorkerThreadEntry();
    void ProcessCompletedRequests(core::LockGuard lock);

    mutable core::Mutex mu_;
    std::atomic<bool> shutdown_{false};

    std::unordered_map<std::string, std::optional<Resolver::ResolutionResult>> results_;
    std::list<core::UniquePtr<ResolveRequest>> activeRequests_;

    core::Thread workerThread_;
};

}  // namespace mithril::http

#endif
