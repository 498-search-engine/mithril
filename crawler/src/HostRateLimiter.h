#ifndef CRAWLER_HOSTRATELIMITER_H
#define CRAWLER_HOSTRATELIMITER_H

#include "core/lru_cache.h"
#include "core/mutex.h"
#include "http/Resolver.h"

#include <string>

namespace mithril {

class HostRateLimiter {
public:
    HostRateLimiter(unsigned long defaultDelayMs);

    long TryLeaseHost(const std::string& host, const std::string& port);
    long TryLeaseHost(const std::string& host, const std::string& port, long now);

    void UnleaseHost(const std::string& host, const std::string& port);
    void UnleaseHost(const std::string& host, const std::string& port, long now);

    long TryUseHost(const std::string& host, const std::string& port);
    long TryUseHost(const std::string& host, const std::string& port, long now);

    void SetHostDelayMs(const std::string& host, const std::string& port, unsigned long delayMs);

private:
    struct Entry {
        bool leased{};
        long earliest{};
        unsigned long delayMs{};
    };

    long TryLeaseHostImpl(const std::string& host, const std::string& port, long now);
    long TryUseHostImpl(const std::string& host, const std::string& port, long now);
    void UnleaseHostImpl(const std::string& host, const std::string& port, long now);

    Entry* GetOrInsert(const std::string& host, const std::string& port);
    const http::ResolvedAddr* GetOrResolve(const std::string& host, const std::string& port, bool& ready);

    mutable core::Mutex mu_;
    unsigned long defaultDelayMs_;

    Entry fallbackEntry_;
    core::LRUCache<http::ResolvedAddr, Entry> m_;
    core::LRUCache<std::string, http::ResolvedAddr> addrs_;
};

}  // namespace mithril

#endif
