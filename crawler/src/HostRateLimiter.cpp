#include "HostRateLimiter.h"

#include "Clock.h"
#include "core/algorithm.h"
#include "core/locks.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>

namespace mithril {

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

HostRateLimiter::HostRateLimiter(unsigned long defaultDelayMs) : defaultDelayMs_(defaultDelayMs), m_(50000) {}

long HostRateLimiter::TryUseHost(std::string_view host) {
    core::LockGuard lock(mu_);
    return TryUseHostImpl(host, MonotonicTimeMs());
}

long HostRateLimiter::TryUseHost(std::string_view host, long now) {
    core::LockGuard lock(mu_);
    return TryUseHostImpl(host, now);
}

long HostRateLimiter::TryUseHostImpl(std::string_view host, long now) {
    auto baseHost = std::string{GetBaseHost(host)};
    auto* it = m_.Find(baseHost);
    if (it == nullptr) {
        auto p = m_.Insert({
            baseHost, Entry{.earliest = 0, .delayMs = defaultDelayMs_}
        });
        assert(p.second);
        it = p.first;
    }

    if (now >= it->second.earliest) {
        it->second.earliest = now + static_cast<long>(it->second.delayMs);
        return 0;
    }

    return it->second.earliest - now;
}


long HostRateLimiter::EarliestForHost(std::string_view host) {
    core::LockGuard lock(mu_);

    auto baseHost = std::string{GetBaseHost(host)};
    auto* it = m_.Find(baseHost);
    if (it == nullptr) {
        auto p = m_.Insert({
            baseHost, Entry{.earliest = 0, .delayMs = defaultDelayMs_}
        });
        assert(p.second);
        it = p.first;
    }
    return it->second.earliest;
}

void HostRateLimiter::SetHostDelayMs(std::string_view host, unsigned long delayMs) {
    core::LockGuard lock(mu_);

    auto baseHost = std::string{GetBaseHost(host)};
    auto* it = m_.Find(baseHost);
    if (it == nullptr) {
        auto p = m_.Insert({
            baseHost, Entry{.earliest = 0, .delayMs = delayMs}
        });
        return;
    } else {
        it->second.delayMs = delayMs;
    }
}

}  // namespace mithril
