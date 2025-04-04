#include "HostRateLimiter.h"

#include "Clock.h"
#include "core/algorithm.h"
#include "core/locks.h"
#include "core/lru_cache.h"
#include "http/Resolver.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace mithril {

constexpr long RateLimitBucketDurationMs = 60000;
constexpr size_t RateLimitBucketMax = 60;

namespace {

std::string_view GetBaseHost(std::string_view host) {
    auto dotCount = core::count(host.begin(), host.end(), '.');
    if (dotCount > 1) {
        size_t seen = 0;
        for (size_t i = 0; i < host.size(); ++i) {
            if (host[i] == '.') {
                ++seen;
                if (seen == dotCount - 1) {
                    return host.substr(i + 1);
                }
            }
        }
    }

    return host;
}

}  // namespace

HostRateLimiter::HostRateLimiter(unsigned long defaultDelayMs)
    : defaultDelayMs_(defaultDelayMs), m_(50000), addrs_(50000) {
    assert(defaultDelayMs > 0);
}

long HostRateLimiter::TryLeaseHost(const std::string& host, const std::string& port, unsigned long delayMs) {
    core::LockGuard lock(mu_);
    return TryLeaseHostImpl(host, port, MonotonicTimeMs(), delayMs);
}

long HostRateLimiter::TryLeaseHost(const std::string& host, const std::string& port, long now, unsigned long delayMs) {
    core::LockGuard lock(mu_);
    return TryLeaseHostImpl(host, port, now, delayMs);
}

void HostRateLimiter::UnleaseHost(const std::string& host, const std::string& port) {
    core::LockGuard lock(mu_);
    UnleaseHostImpl(host, port, MonotonicTimeMs());
}

void HostRateLimiter::UnleaseHost(const std::string& host, const std::string& port, long now) {
    core::LockGuard lock(mu_);
    UnleaseHostImpl(host, port, now);
}

void HostRateLimiter::UnleaseHostImpl(const std::string& host, const std::string& port, long now) {
    auto* entry = GetOrInsert(host, port);
    if (entry == nullptr || entry == &fallbackEntry_) {
        return;
    }

    assert(entry->leased);
    entry->earliest = now + static_cast<long>(entry->delayAfterUnlease);
    entry->leased = false;
}

long HostRateLimiter::TryLeaseHostImpl(const std::string& host,
                                       const std::string& port,
                                       long now,
                                       unsigned long delayMs) {
    auto* entry = GetOrInsert(host, port);
    if (entry == nullptr) {
        return 10;
    } else if (entry == &fallbackEntry_) {
        return 0;
    }

    if (entry->leased) {
        return core::max(entry->earliest - now, 5L);
    } else if (now < entry->earliest) {
        // Need to wait
        return entry->earliest - now;
    }

    auto bucketWait = TryIncrementBucket(*entry, now);
    if (bucketWait > 0) {
        return bucketWait;
    }

    entry->earliest = now + static_cast<long>(defaultDelayMs_);
    entry->leased = true;
    entry->delayAfterUnlease = delayMs;

    return 0;
}

long HostRateLimiter::TryUseHost(const std::string& host, const std::string& port) {
    core::LockGuard lock(mu_);
    return TryUseHostImpl(host, port, MonotonicTimeMs());
}

long HostRateLimiter::TryUseHost(const std::string& host, const std::string& port, long now) {
    core::LockGuard lock(mu_);
    return TryUseHostImpl(host, port, now);
}

long HostRateLimiter::TryUseHostImpl(const std::string& host, const std::string& port, long now) {
    auto* entry = GetOrInsert(host, port);
    if (entry == nullptr) {
        return 10;
    } else if (entry == &fallbackEntry_) {
        return 0;
    }

    if (entry->leased) {
        return now < entry->earliest ? entry->earliest - now : 10;  // 10 ms, idk
    } else if (now < entry->earliest) {
        // Need to wait
        return entry->earliest - now;
    }

    auto bucketWait = TryIncrementBucket(*entry, now);
    if (bucketWait > 0) {
        return bucketWait;
    }

    entry->earliest = now + static_cast<long>(defaultDelayMs_);
    return 0;
}

HostRateLimiter::Entry* HostRateLimiter::GetOrInsert(const std::string& host, const std::string& port) {
    assert(!host.empty());
    assert(!port.empty());

    bool ready = false;
    const auto* resolved = GetOrResolve(host, port, ready);
    if (!ready) {
        // Still waiting
        return nullptr;
    }

    if (resolved == nullptr) {
        // Resolved to failure
        return &fallbackEntry_;
    }

    auto* it = m_.Find(*resolved);
    if (it == nullptr) {
        auto p = m_.Insert({
            *resolved, Entry{.leased = false, .earliest = 0}
        });
        assert(p.second);
        it = p.first;
    }

    return &it->second;
}

const http::ResolvedAddr* HostRateLimiter::GetOrResolve(const std::string& host, const std::string& port, bool& ready) {
    assert(!host.empty());
    assert(!port.empty());

    auto combined = host + ':' + port;
    auto* existing = addrs_.Find(combined);
    if (existing != nullptr) {
        ready = true;
        return &existing->second;
    }

    http::Resolver::ResolutionResult res;
    bool resolved = http::ApplicationResolver->Resolve(host, port, res);
    if (!resolved) {
        // Still waiting
        ready = false;
        return nullptr;
    } else if (res.status != 0 || !res.addr.has_value()) {
        // Resolved but got error response
        ready = true;
        return nullptr;
    }

    auto inserted = addrs_.Insert({combined, std::move(*res.addr)});
    assert(inserted.second);
    ready = true;
    return &inserted.first->second;
}

long HostRateLimiter::TryIncrementBucket(Entry& entry, long now) {
    if (now - entry.bucketStart >= RateLimitBucketDurationMs) {
        // reset bucket
        entry.bucketStart = now;
        entry.bucketCount = 0;
    }
    if (entry.bucketCount >= RateLimitBucketMax) {
        // exceeded 60 requests in a minute, wait til bucket resets
        return RateLimitBucketDurationMs - (now - entry.bucketStart);
    }
    ++entry.bucketCount;
    return 0;
}

}  // namespace mithril
