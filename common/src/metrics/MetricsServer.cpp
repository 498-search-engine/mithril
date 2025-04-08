#include "metrics/MetricsServer.h"

#include "ThreadSync.h"
#include "metrics/Metrics.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>

namespace mithril::metrics {


MetricsServer::MetricsServer(uint16_t port) : port_(port) {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == -1) {
        spdlog::error("failed to create metrics socket: {}", strerror(errno));
        throw std::runtime_error("failed to create metrics socket");
    }

    int reuse = 1;
    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        close(sock_);
        spdlog::error("failed to set SO_REUSEADDR on metrics socket: {}", strerror(errno));
        throw std::runtime_error("failed to configure metrics socket");
    }

    struct sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(port);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int status = bind(sock_, (struct sockaddr*)&bindAddr, sizeof(bindAddr));
    if (status == -1) {
        close(sock_);
        spdlog::error("failed to bind metrics socket: {}", strerror(errno));
        throw std::runtime_error("failed to bind metrics socket");
    }
}

MetricsServer::~MetricsServer() {
    if (sock_ != -1) {
        close(sock_);
        sock_ = -1;
    }
}

void MetricsServer::Register(const RenderableMetric* metric) {
    metrics_.push_back(metric);
}

void MetricsServer::Run(ThreadSync& sync) {
    int status = listen(sock_, SOMAXCONN);
    if (status < 0) {
        spdlog::error("metrics socket failed to listen: {}", strerror(errno));
        return;
    }

    struct sockaddr_in talkingAddress {};
    socklen_t talkingAddressLen = sizeof(talkingAddress);

    fd_set readfds;

    spdlog::info("metrics server listening at :{}", port_);

    while (!sync.ShouldShutdown()) {
        FD_ZERO(&readfds);
        FD_SET(sock_, &readfds);

        struct timeval timeout {
            .tv_sec = 0, .tv_usec = 500 * 1000,
        };
        int activity = select(sock_ + 1, &readfds, nullptr, nullptr, &timeout);
        if (activity == -1) {
            spdlog::error("select on socket: {}", strerror(errno));
            return;
        } else if (activity == 0) {
            continue;
        }

        if (FD_ISSET(sock_, &readfds)) {
            int talkSock = accept(sock_, (struct sockaddr*)&talkingAddress, &talkingAddressLen);
            if (talkSock == -1) {
                spdlog::warn("metrics server failed to accept connection: {}", strerror(errno));
                continue;
            }
            HandleConnection(talkSock);
            close(talkSock);
        }
    }

    close(sock_);
    sock_ = -1;
    spdlog::info("metrics server terminating");
}

void MetricsServer::HandleConnection(int fd) {
    char buffer[8192];
    ssize_t bytes = 0;
    size_t total = 0;
    bool foundEnd = false;

    while (total < sizeof(buffer) - 1) {
        bytes = recv(fd, buffer + total, sizeof(buffer) - total - 1, 0);
        if (bytes < 0) {
            spdlog::warn("metrics server: read request: {}", strerror(errno));
            return;
        } else if (bytes == 0) {
            // EOF
            return;
        }
        total += bytes;
        buffer[total] = '\0';
        // find end of headers
        if (strstr(buffer, "\r\n\r\n")) {
            foundEnd = true;
            break;
        }
    }

    if (!foundEnd) {
        spdlog::warn("metrics server request was malformed or too long");
        return;
    }

    // We don't actually care about the content of the request. This server does
    // one thing and one thing only: renders metrics.

    auto res = RenderResponse();
    ssize_t sent = 0;
    do {
        auto justSent = send(fd, res.data() + sent, res.size() - sent, 0);
        if (justSent <= 0) {
            return;
        }
        sent += justSent;
    } while (sent < res.size());
}


std::vector<char> MetricsServer::RenderResponse() {
    auto metrics = RenderMetrics();
    std::vector<char> res;
    res.reserve(metrics.size() + 500);

    auto contentLengthHeader = "Content-Length: " + std::to_string(metrics.length()) + "\r\n";

    std::string headers = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain; version=0.0.4\r\n"
                          "Connection: close\r\n" +
                          contentLengthHeader + "\r\n";
    res.insert(res.end(), headers.begin(), headers.end());
    res.insert(res.end(), metrics.begin(), metrics.end());
    return res;
}

std::string MetricsServer::RenderMetrics() {
    std::string res;
    for (const auto* metric : metrics_) {
        metric->Render(res);
    }
    return res;
}


}  // namespace mithril::metrics
