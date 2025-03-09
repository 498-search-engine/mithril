#include "metrics/Metrics.h"

#include "core/locks.h"
#include "core/memory.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>


namespace mithril::metrics {

namespace {

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

    if (val - std::floor(val) <= 0.0001) {
        out.append(std::to_string(static_cast<int>(val)));
    } else {
        out.append(std::to_string(val));
    }

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

}  // namespace mithril::metrics
