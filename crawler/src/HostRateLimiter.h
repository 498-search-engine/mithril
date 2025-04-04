#ifndef CRAWLER_HOSTRATELIMITER_H
#define CRAWLER_HOSTRATELIMITER_H

#include "core/lru_cache.h"
#include "core/mutex.h"

#include <string>
#include <string_view>

namespace mithril {

class HostRateLimiter {
public:
    HostRateLimiter(unsigned long defaultDelayMs);

    long TryUseHost(std::string_view host);
    long TryUseHost(std::string_view host, long now);

    long EarliestForHost(std::string_view host);
    void SetHostDelayMs(std::string_view host, unsigned long delayMs);

private:
    struct Entry {
        long earliest{};
        unsigned long delayMs{};
    };

    long TryUseHostImpl(std::string_view host, long now);

    mutable core::Mutex mu_;
    unsigned long defaultDelayMs_;
    core::LRUCache<std::string, Entry> m_;
};

}  // namespace mithril

#endif
