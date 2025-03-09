#ifndef COMMON_METRICS_MODEL_H
#define COMMON_METRICS_MODEL_H

#include "core/memory.h"
#include "core/mutex.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>

namespace std {
template<>
struct hash<map<string, string>> {
    size_t operator()(const map<string, string>& m) const {
        size_t seed = 0;
        for (const auto& pair : m) {
            // Combine the hash of the key and value
            seed ^= hash<string>()(pair.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>()(pair.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
}  // namespace std

inline bool operator==(const std::map<std::string, std::string>& lhs, const std::map<std::string, std::string>& rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

namespace mithril::metrics {

constexpr const char* MetricTypeCounter = "counter";
constexpr const char* MetricTypeGauge = "gauge";

using Labels = std::map<std::string, std::string>;
using Label = Labels::value_type;

class MetricValue {
public:
    MetricValue();

    void Inc();
    void Dec();
    void Add(double delta);
    void Sub(double delta);
    void Set(double val);
    void Set(size_t val);
    void Zero();

    double Value() const;

private:
    std::atomic<double> v_;
};

struct MetricDefinition {
    std::string name;
    std::string type;
    std::string help;
};

class Metric {
public:
    Metric(std::string name, std::string type, std::string help);

    void Render(std::string& out) const;

    MetricValue& WithLabels(const Labels& labels);
    MetricValue& Get();

private:
    mutable core::Mutex mu_;

    MetricDefinition def_;
    core::UniquePtr<MetricValue> emptyLabelMetric_;
    std::unordered_map<Labels, core::UniquePtr<MetricValue>> rawMetrics_;
};

}  // namespace mithril::metrics


#endif
