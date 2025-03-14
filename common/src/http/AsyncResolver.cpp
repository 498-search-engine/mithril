#include "http/AsyncResolver.h"

#include "core/locks.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <netdb.h>
#include <sys/sysinfo.h>

namespace mithril::http {

AsyncResolver::AsyncResolver() : AsyncResolver(std::clamp(get_nprocs() * 2, 4, 16)) {}

AsyncResolver::AsyncResolver(size_t workers) {
    spdlog::info("pooled async resolver starting with {} workers", workers);
    workers_.reserve(workers);
    for (size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this, i] { this->WorkerThreadEntry(); });
    }
}

AsyncResolver::~AsyncResolver() {
    {
        core::LockGuard lock(activeMu_);
        shutdown_ = true;
    }
    requestsCv_.Broadcast();
    for (auto& thread : workers_) {
        if (thread.Joinable()) {
            thread.Join();
        }
    }
}

bool AsyncResolver::Resolve(const std::string& host, const std::string& port, Resolver::ResolutionResult& result) {
    core::LockGuard lock(resultsMu_);
    auto key = host + ':' + port;
    auto it = results_.find(key);
    if (it == results_.end()) {
        results_[key] = std::nullopt;
        lock.Unlock();
        StartResolve(host, port, key);
        return false;
    }

    if (!it->second.has_value()) {
        // Still waiting for resolution
        return false;
    }

    result = *it->second;
    return true;
}

void AsyncResolver::StartResolve(const std::string& host, const std::string& port, const std::string& key) {
    core::LockGuard lock(activeMu_);
    activeRequests_.push(ResolveRequest{
        .host = host,
        .port = port,
        .key = key,
    });
    requestsCv_.Signal();
}

void AsyncResolver::WorkerThreadEntry() {
    core::LockGuard lock(activeMu_);
    while (!shutdown_) {
        requestsCv_.Wait(lock, [this] { return shutdown_ || !activeRequests_.empty(); });
        if (shutdown_) {
            return;
        }

        assert(!activeRequests_.empty());
        auto req = std::move(activeRequests_.front());
        activeRequests_.pop();
        lock.Unlock();
        ResolveSync(req);
        lock.Lock();
    }
}

void AsyncResolver::ResolveSync(const ResolveRequest& req) {
    ResolutionResult result;

    struct addrinfo* address = nullptr;
    struct addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int status = getaddrinfo(req.host.c_str(), req.port.c_str(), &hints, &address);
    if (status != 0) {
        result.status = status;
        result.addr = std::nullopt;
    } else if (address == nullptr) {
        result.status = EAI_SYSTEM;
        result.addr = std::nullopt;
    } else {
        result.status = status;
        result.addr = ResolvedAddr(address);
        freeaddrinfo(address);
    }

    {
        core::LockGuard lock(resultsMu_);
        results_[req.key] = std::move(result);
    }
}

}  // namespace mithril::http
