#ifndef COMMON_HTTP_RESOLVER_H
#define COMMON_HTTP_RESOLVER_H

#include "core/memory.h"

#include <cstdint>
#include <netdb.h>
#include <optional>
#include <string>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>

namespace mithril::http {

class ResolvedAddr {
public:
    ResolvedAddr();
    explicit ResolvedAddr(const addrinfo* src);

    ~ResolvedAddr() = default;

    ResolvedAddr(const ResolvedAddr& other);
    ResolvedAddr& operator=(const ResolvedAddr& other);

    ResolvedAddr(ResolvedAddr&& other) noexcept;
    ResolvedAddr& operator=(ResolvedAddr&& other) noexcept;

    const addrinfo* AddrInfo() const;

private:
    addrinfo info_{};
    std::vector<char> addrStorage_;
    std::vector<char> canonnameStorage_;
};

bool operator==(const ResolvedAddr& lhs, const ResolvedAddr& rhs);

class Resolver {
public:
    virtual ~Resolver() = default;

    struct ResolutionResult {
        int status{0};
        std::optional<ResolvedAddr> addr;
    };

    virtual bool Resolve(const std::string& host, const std::string& port, ResolutionResult& result) = 0;
};

inline core::UniquePtr<Resolver> ApplicationResolver{};

}  // namespace mithril::http

#include <cstring>
#include <functional>

namespace std {

template<>
struct hash<mithril::http::ResolvedAddr> {
    std::size_t operator()(const mithril::http::ResolvedAddr& addr) const {
        const addrinfo* info = addr.AddrInfo();
        if (!info || !info->ai_addr) {
            return 0;
        }

        // Hash based on address family and the actual address
        std::size_t h = std::hash<int>{}(info->ai_family);

        if (info->ai_family == AF_INET) {
            // IPv4 address
            const auto* ipv4 = reinterpret_cast<const struct sockaddr_in*>(info->ai_addr);
            h ^= std::hash<uint32_t>{}(ipv4->sin_addr.s_addr) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint16_t>{}(ipv4->sin_port) + 0x9e3779b9 + (h << 6) + (h >> 2);
        } else {
            // Other address family, hash the raw bytes
            const char* addrBytes = reinterpret_cast<const char*>(info->ai_addr);
            for (size_t i = 0; i < info->ai_addrlen; i++) {
                h ^= std::hash<char>{}(addrBytes[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
        }

        return h;
    }
};

}  // namespace std

#endif
