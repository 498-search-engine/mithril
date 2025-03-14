#ifndef COMMON_HTTP_RESOLVER_H
#define COMMON_HTTP_RESOLVER_H

#include "core/memory.h"

#include <netdb.h>
#include <optional>
#include <string>
#include <vector>

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

#endif
