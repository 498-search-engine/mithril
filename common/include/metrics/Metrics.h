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
#include <vector>

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
constexpr const char* MetricTypeHistogram = "histogram";

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

class RenderableMetric {
public:
    virtual void Render(std::string& out) const = 0;
};

class Metric : public RenderableMetric {
public:
    Metric(std::string name, std::string type, std::string help);

    void Render(std::string& out) const override;

    void Inc();
    void Dec();
    void Add(double delta);
    void Sub(double delta);
    void Set(double val);
    void Set(size_t val);
    void Zero();

    MetricValue& WithLabels(const Labels& labels);

private:
    MetricValue& Get();

    mutable core::Mutex mu_;

    MetricDefinition def_;
    core::UniquePtr<MetricValue> emptyLabelMetric_;
    std::unordered_map<Labels, core::UniquePtr<MetricValue>> rawMetrics_;
};

class HistogramMetric : public RenderableMetric {
public:
    HistogramMetric(std::string name, std::string help, std::vector<double> buckets);

    void Observe(double value);

    void Render(std::string& out) const override;

private:
    mutable core::Mutex mu_;

    std::string name_;
    std::string help_;
    std::string bucketStr_;

    std::vector<double> buckets_;
    std::vector<double> bucketValues_;
    std::vector<Labels> bucketLabels_;

    double sum_;
    double count_;
};

std::vector<double> ExponentialBuckets(double start, double multiple, size_t count);

}  // namespace mithril::metrics


#endif
