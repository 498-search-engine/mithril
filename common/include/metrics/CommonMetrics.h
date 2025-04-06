#ifndef COMMON_COMMONMETRICS_H
#define COMMON_COMMONMETRICS_H

#include "metrics/Metrics.h"
#include "metrics/MetricsServer.h"

namespace mithril {

using namespace metrics;

inline auto DNSCacheHits = Metric{
    "common_dns_cache_hits",
    MetricTypeCounter,
    "Number of DNS lookup cache hits",
};

inline auto DNSCacheMisses = Metric{
    "common_dns_cache_misses",
    MetricTypeCounter,
    "Number of DNS lookup cache misses",
};

inline auto RegisterCommonMetrics(MetricsServer& server) {
    server.Register(&DNSCacheHits);
    server.Register(&DNSCacheMisses);
}

}  // namespace mithril

#endif
