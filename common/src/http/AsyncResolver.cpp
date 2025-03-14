#include "http/AsyncResolver.h"

#include "core/locks.h"
#include "core/memory.h"
#include "core/thread.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>
#include <netdb.h>
#include <optional>
#include <unistd.h>

namespace mithril::http {

AsyncResolver::AsyncResolver() : workerThread_(core::Thread([this] { this->WorkerThreadEntry(); })) {}

AsyncResolver::~AsyncResolver() {
    if (workerThread_.Joinable()) {
        shutdown_.store(true);
        workerThread_.Join();
    }
}

bool AsyncResolver::Resolve(const std::string& host, const std::string& port, ResolutionResult& result) {
    core::LockGuard lock(mu_);
    auto key = host + ':' + port;
    auto it = results_.find(key);
    if (it == results_.end()) {
        results_[key] = std::nullopt;
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
    auto req = core::MakeUnique<ResolveRequest>(ResolveRequest{});
    auto* reqPtr = req.Get();

    req->host = host;
    req->port = port;
    req->key = key;

    memset(&req->request, 0, sizeof(gaicb));
    req->request.ar_name = req->host.c_str();
    req->request.ar_service = req->port.c_str();

    memset(&req->hints, 0, sizeof(addrinfo));
    req->hints.ai_family = AF_INET;
    req->hints.ai_socktype = SOCK_STREAM;
    req->hints.ai_protocol = IPPROTO_TCP;

    req->request.ar_request = &req->hints;
    req->request.ar_result = nullptr;

    activeRequests_.push_back(std::move(req));

    auto* gaicbReq = &reqPtr->request;
    int status = getaddrinfo_a(GAI_NOWAIT, &gaicbReq, 1, nullptr);
    if (status != 0) {
        results_[key] = Resolver::ResolutionResult{
            .status = status,
            .addr = std::nullopt,
        };

        auto it = std::find_if(
            activeRequests_.begin(), activeRequests_.end(), [reqPtr](const auto& e) { return e.Get() == reqPtr; });
        if (it != activeRequests_.end()) {
            activeRequests_.erase(it);
        }
    }
}

void AsyncResolver::WorkerThreadEntry() {
    spdlog::info("async resolver worker starting");
    constexpr auto Interval = timespec{.tv_sec = 0, .tv_nsec = 10L * 1000L * 1000L};
    while (!shutdown_.load()) {
        core::LockGuard lock(mu_);
        ProcessCompletedRequests(std::move(lock));
        usleep(10000);
    }
}

void AsyncResolver::ProcessCompletedRequests(core::LockGuard lock) {
    std::vector<std::pair<int, core::UniquePtr<ResolveRequest>>> completed;

    if (activeRequests_.empty()) {
        return;
    }

    auto it = activeRequests_.begin();
    while (it != activeRequests_.end()) {
        int status = gai_error(&it->Get()->request);
        if (status == EAI_INPROGRESS) {
            ++it;
            continue;
        }

        completed.emplace_back(status, std::move(*it));
        it = activeRequests_.erase(it);
    }

    lock.Unlock();

    for (auto& [status, req] : completed) {
        ResolutionResult result;

        if (status != 0) {
            result.status = status;
            result.addr = std::nullopt;
        } else if (req->request.ar_result == nullptr) {
            result.status = EAI_SYSTEM;
            result.addr = std::nullopt;
        } else {
            result.status = status;
            result.addr = ResolvedAddr(req->request.ar_result);
            freeaddrinfo(req->request.ar_result);
        }

        lock.Lock();
        results_[req->key] = result;
        lock.Unlock();

        req.Reset(nullptr);
    }
}


}  // namespace mithril::http
