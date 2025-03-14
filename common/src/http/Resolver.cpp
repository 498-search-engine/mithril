#include "http/Resolver.h"

#include <cstring>
#include <netdb.h>
#include <vector>

namespace mithril::http {

ResolvedAddr::ResolvedAddr() {
    std::memset(&info_, 0, sizeof(addrinfo));
}

ResolvedAddr::ResolvedAddr(const addrinfo* src) {
    if (!src) {
        std::memset(&info_, 0, sizeof(addrinfo));
        return;
    }

    // Copy the main structure, only using first resolved address
    info_ = *src;
    info_.ai_next = nullptr;

    // Deep copy sockaddr
    if (src->ai_addr && src->ai_addrlen > 0) {
        addrStorage_.resize(src->ai_addrlen);
        std::memcpy(addrStorage_.data(), src->ai_addr, src->ai_addrlen);
        info_.ai_addr = reinterpret_cast<sockaddr*>(addrStorage_.data());
    } else {
        info_.ai_addr = nullptr;
    }

    // Deep copy canonical name if present
    if (src->ai_canonname != nullptr) {
        auto len = std::strlen(src->ai_canonname) + 1;
        canonnameStorage_.resize(len);
        std::memcpy(canonnameStorage_.data(), src->ai_canonname, len);
        info_.ai_canonname = canonnameStorage_.data();
    } else {
        info_.ai_canonname = nullptr;
    }
}

ResolvedAddr::ResolvedAddr(const ResolvedAddr& other)
    : info_(other.info_), addrStorage_(other.addrStorage_), canonnameStorage_(other.canonnameStorage_) {
    // Update pointers to point to our copies
    if (!addrStorage_.empty()) {
        info_.ai_addr = reinterpret_cast<sockaddr*>(addrStorage_.data());
    } else {
        info_.ai_addr = nullptr;
    }

    if (!canonnameStorage_.empty()) {
        info_.ai_canonname = canonnameStorage_.data();
    } else {
        info_.ai_canonname = nullptr;
    }

    info_.ai_next = nullptr;
}

ResolvedAddr& ResolvedAddr::operator=(const ResolvedAddr& other) {
    if (this != &other) {
        info_ = other.info_;
        addrStorage_ = other.addrStorage_;
        canonnameStorage_ = other.canonnameStorage_;

        // Update pointers to point to our copies
        if (!addrStorage_.empty()) {
            info_.ai_addr = reinterpret_cast<sockaddr*>(addrStorage_.data());
        } else {
            info_.ai_addr = nullptr;
        }

        if (!canonnameStorage_.empty()) {
            info_.ai_canonname = canonnameStorage_.data();
        } else {
            info_.ai_canonname = nullptr;
        }

        info_.ai_next = nullptr;
    }
    return *this;
}

ResolvedAddr::ResolvedAddr(ResolvedAddr&& other) noexcept
    : info_(other.info_),
      addrStorage_(std::move(other.addrStorage_)),
      canonnameStorage_(std::move(other.canonnameStorage_)) {
    // Update pointers to point to our moved data
    if (!addrStorage_.empty()) {
        info_.ai_addr = reinterpret_cast<sockaddr*>(addrStorage_.data());
    }

    if (!canonnameStorage_.empty()) {
        info_.ai_canonname = canonnameStorage_.data();
    }

    other.info_.ai_addr = nullptr;
    other.info_.ai_canonname = nullptr;
    other.info_.ai_next = nullptr;
}

ResolvedAddr& ResolvedAddr::operator=(ResolvedAddr&& other) noexcept {
    if (this != &other) {
        info_ = other.info_;
        addrStorage_ = std::move(other.addrStorage_);
        canonnameStorage_ = std::move(other.canonnameStorage_);

        // Update pointers to point to our moved data
        if (!addrStorage_.empty()) {
            info_.ai_addr = reinterpret_cast<sockaddr*>(addrStorage_.data());
        } else {
            info_.ai_addr = nullptr;
        }

        if (!canonnameStorage_.empty()) {
            info_.ai_canonname = canonnameStorage_.data();
        } else {
            info_.ai_canonname = nullptr;
        }

        other.info_.ai_addr = nullptr;
        other.info_.ai_canonname = nullptr;
        other.info_.ai_next = nullptr;
    }
    return *this;
}

const addrinfo* ResolvedAddr::AddrInfo() const {
    return &info_;
}

}  // namespace mithril::http
