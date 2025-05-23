#ifndef CRAWLER_HOSTRATELIMITER_H
#define CRAWLER_HOSTRATELIMITER_H

#include "core/lru_cache.h"
#include "core/mutex.h"
#include "http/Resolver.h"

#include <cstddef>
#include <string>

namespace mithril {

class HostRateLimiter {
public:
    HostRateLimiter(unsigned long defaultDelayMs,
                    long rateLimitBucketDurationMs,
                    unsigned long rateLimitBucketRequestCount);

    long TryUseHost(const std::string& host, const std::string& port);
    long TryUseHost(const std::string& host, const std::string& port, long now);

private:
    struct Entry {
        long bucketStart{};
        size_t bucketCount{};
    };

    long TryUseHostImpl(const std::string& host, const std::string& port, long now);

    Entry* GetOrInsert(const std::string& host, const std::string& port);
    const http::ResolvedAddr* GetOrResolve(const std::string& host, const std::string& port, bool& ready);

    long TryIncrementBucket(Entry& entry, long now) const;

    mutable core::Mutex mu_;
    unsigned long defaultDelayMs_;
    long rateLimitBucketDurationMs_;
    unsigned long rateLimitBucketRequestCount_;

    Entry fallbackEntry_;
    core::LRUCache<http::ResolvedAddr, Entry> m_;
    core::LRUCache<std::string, http::ResolvedAddr> addrs_;

    size_t leasedCount_{0};
};

}  // namespace mithril

#endif
