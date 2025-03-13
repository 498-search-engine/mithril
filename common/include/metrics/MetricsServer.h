#ifndef COMMON_METRICS_METRICSSERVER_H
#define COMMON_METRICS_METRICSSERVER_H

#include "ThreadSync.h"
#include "metrics/Metrics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mithril::metrics {

class MetricsServer {
public:
    MetricsServer(uint16_t port);
    ~MetricsServer();

    void Register(const RenderableMetric* metric);
    void Run(ThreadSync& sync);

private:
    void HandleConnection(int fd);
    std::vector<char> RenderResponse();
    std::string RenderMetrics();

    uint16_t port_;
    int sock_;

    std::vector<const RenderableMetric*> metrics_;
};

}  // namespace mithril::metrics

#endif
