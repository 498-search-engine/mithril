#include "Clock.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

long MonotonicTime() {
    struct timespec tp {};
    int status = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (status < 0) [[unlikely]] {
        perror("clock_gettime");
        exit(1);
    }
    return tp.tv_sec;
}
