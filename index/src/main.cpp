#include "InvertedIndex.h"

#include <iostream>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
#if !defined(NDEBUG)
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    signal(SIGPIPE, SIG_IGN);

    std::cout << "mithril index" << std::endl;

    return 0;
}
