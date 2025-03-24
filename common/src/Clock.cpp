#include "Clock.h"

#include "spdlog/spdlog.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>

long MonotonicTime() {
    struct timespec tp {};
    int status = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (status < 0) [[unlikely]] {
        spdlog::critical("clock_gettime failed: {}", strerror(errno));
        exit(1);
    }
    return tp.tv_sec;
}

long MonotonicTimeMs() {
    struct timespec tp {};
    int status = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (status < 0) [[unlikely]] {
        spdlog::critical("clock_gettime failed: {}", strerror(errno));
        exit(1);
    }
    return (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);
}
