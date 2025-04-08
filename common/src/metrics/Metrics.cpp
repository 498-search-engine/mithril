#include "metrics/Metrics.h"

#include "core/locks.h"
#include "core/memory.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace mithril::metrics {

namespace {

constexpr double Epsilon = 0.000001;

void RenderPrometheusString(const std::string& s, std::string& out) {
    out.push_back('"');
    for (char c : s) {
        if (c == '\\') {
            out.push_back('\\');
            out.push_back('\\');
        } else if (c == '"') {
            out.push_back('\\');
            out.push_back('"');
        } else if (c == '\n') {
            out.push_back('\\');
            out.push_back('n');
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
}

std::string StringOfDouble(double val) {
    if (val - std::floor(val) <= Epsilon && val < static_cast<double>(std::numeric_limits<long>::max())) {
        return std::to_string(static_cast<long>(val));
    } else {
        return std::to_string(val);
    }
}

void RenderMetricValue(const std::string& name, const Labels& labels, double val, std::string& out) {
    // metric_name [
    //   "{" label_name "=" `"` label_value `"` { "," label_name "=" `"` label_value `"` } [ "," ] "}"
    // ] value [ timestamp ]
    // http_requests_total{method="post",code="200"} 1027
    out.append(name);
    if (!labels.empty()) {
        out.push_back('{');
        size_t i = 0;
        for (const auto& label : labels) {
            out.append(label.first);
            out.push_back('=');
            RenderPrometheusString(label.second, out);
            if (i != labels.size() - 1) {
                out.push_back(',');
            }
        }
        out.push_back('}');
    }
    out.push_back(' ');
    out.append(StringOfDouble(val));
    out.push_back('\n');
}

}  // namespace

MetricValue::MetricValue() : v_(0.0) {}

void MetricValue::Inc() {
    Add(1.0);
}

void MetricValue::Dec() {
    Sub(1.0);
}

void MetricValue::Add(double delta) {
    v_.fetch_add(delta);
}

void MetricValue::Sub(double delta) {
    v_.fetch_sub(delta);
}

void MetricValue::Set(double val) {
    v_.store(val);
}

void MetricValue::Set(size_t val) {
    v_.store(static_cast<double>(val));
}

void MetricValue::Zero() {
    v_.store(0.0);
}

double MetricValue::Value() const {
    return v_.load();
}

Metric::Metric(std::string name, std::string type, std::string help)
    : def_(MetricDefinition{.name = std::move(name), .type = std::move(type), .help = std::move(help)}) {}

void Metric::Render(std::string& out) const {
    core::LockGuard lock(mu_);
    // # HELP http_requests_total The total number of HTTP requests.
    if (!def_.help.empty()) {
        out.append("# HELP ");
        out.append(def_.name);
        out.push_back(' ');
        out.append(def_.help);
        out.push_back('\n');
    }
    // # TYPE http_requests_total counter
    if (!def_.type.empty()) {
        out.append("# TYPE ");
        out.append(def_.name);
        out.push_back(' ');
        out.append(def_.type);
        out.push_back('\n');
    }

    if (rawMetrics_.empty() && emptyLabelMetric_.Get() == nullptr) {
        // Default of 0
        out.append(def_.name);
        out.push_back(' ');
        out.push_back('0');
        out.push_back('\n');
    } else {
        if (emptyLabelMetric_.Get() != nullptr) {
            RenderMetricValue(def_.name, {}, emptyLabelMetric_->Value(), out);
        }
        for (const auto& entry : rawMetrics_) {
            RenderMetricValue(def_.name, entry.first, entry.second->Value(), out);
        }
    }
}

MetricValue& Metric::WithLabels(const Labels& labels) {
    if (labels.empty()) {
        return Get();
    }

    core::LockGuard lock(mu_);
    auto it = rawMetrics_.find(labels);
    if (it != rawMetrics_.end()) {
        return *it->second;
    }

    auto res = rawMetrics_.insert({labels, core::UniquePtr<MetricValue>(new MetricValue{})});
    assert(res.second);
    return *res.first->second;
}

MetricValue& Metric::Get() {
    core::LockGuard lock(mu_);
    if (emptyLabelMetric_.Get() == nullptr) [[unlikely]] {
        emptyLabelMetric_ = core::UniquePtr<MetricValue>(new MetricValue{});
    }
    return *emptyLabelMetric_;
}

void Metric::Inc() {
    Get().Inc();
}

void Metric::Dec() {
    Get().Dec();
}

void Metric::Add(double delta) {
    Get().Add(delta);
}

void Metric::Sub(double delta) {
    Get().Sub(delta);
}

void Metric::Set(double val) {
    Get().Set(val);
}

void Metric::Set(size_t val) {
    Get().Set(val);
}

void Metric::Zero() {
    Get().Zero();
}

HistogramMetric::HistogramMetric(std::string name, std::string help, std::vector<double> buckets)
    : name_(std::move(name)),
      help_(std::move(help)),
      bucketStr_(name_ + "_bucket"),
      buckets_(std::move(buckets)),
      sum_(0.0),
      count_(0.0) {
    if (buckets_.empty()) {
        throw std::runtime_error("histogram must have at least one bucket");
    }

    std::sort(buckets_.begin(), buckets_.end());
    bucketValues_.resize(buckets_.size() + 1, 0.0);
    bucketLabels_.reserve(buckets_.size() + 1);
    for (double bound : buckets_) {
        bucketLabels_.push_back({
            {"le", StringOfDouble(bound)}
        });
    }
    bucketLabels_.push_back({
        {"le", "+Inf"}
    });
}

void HistogramMetric::Observe(double value) {
    core::LockGuard lock(mu_);

    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) {
            bucketValues_[i] += 1.0;
        }
    }

    // Count in the +Inf bucket too
    bucketValues_.back() += 1.0;

    sum_ += value;
    count_ += 1.0;
}

void HistogramMetric::Observe(size_t value) {
    Observe(static_cast<double>(value));
}

void HistogramMetric::Render(std::string& out) const {
    core::LockGuard lock(mu_);
    // # HELP http_request_duration_seconds HTTP request duration in seconds.
    if (!help_.empty()) {
        out.append("# HELP ");
        out.append(name_);
        out.push_back(' ');
        out.append(help_);
        out.push_back('\n');
    }

    // # TYPE http_request_duration_seconds histogram
    out.append("# TYPE ");
    out.append(name_);
    out.push_back(' ');
    out.append(MetricTypeHistogram);
    out.push_back('\n');

    for (size_t i = 0; i < bucketValues_.size(); ++i) {
        RenderMetricValue(bucketStr_, bucketLabels_[i], bucketValues_[i], out);
    }

    RenderMetricValue(name_ + "_sum", {}, sum_, out);
    RenderMetricValue(name_ + "_count", {}, count_, out);
}

std::vector<double> ExponentialBuckets(double start, double multiple, size_t count) {
    std::vector<double> res;
    res.reserve(count);

    double v = start;
    for (size_t i = 0; i < count; ++i) {
        res.push_back(v);
        v *= multiple;
    }

    return res;
}

std::vector<double> LinearBuckets(double start, double amount, size_t count) {
    std::vector<double> res;
    res.reserve(count);

    double v = start;
    for (size_t i = 0; i < count; ++i) {
        res.push_back(v);
        v += amount;
    }

    return res;
}

}  // namespace mithril::metrics
