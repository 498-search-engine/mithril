#include "http/SyncResolver.h"

#include "http/Resolver.h"

#include <cstring>
#include <optional>

namespace mithril::http {

bool SyncResolver::Resolve(const std::string& host, const std::string& port, Resolver::ResolutionResult& result) {
    struct addrinfo* address = nullptr;
    struct addrinfo hints {};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &address);
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

    return true;
}

}  // namespace mithril::http
