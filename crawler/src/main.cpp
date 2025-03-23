#include "Config.h"
#include "Coordinator.h"
#include "core/memory.h"
#include "http/AsyncResolver.h"
#include "http/Resolver.h"
#include "http/SSL.h"

#include <csignal>
#include <exception>
#include <iostream>
#include <string>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    mithril::http::InitializeSSL();
    mithril::http::ApplicationResolver = core::UniquePtr<mithril::http::Resolver>(new mithril::http::AsyncResolver{});

    try {
        auto config = mithril::LoadConfigFromFile(argc > 1 ? argv[1] : "crawler.conf");
        auto logLevel = spdlog::level::from_str(config.log_level);
        spdlog::set_level(logLevel);
        if (logLevel == spdlog::level::off) {
            std::cout << "logging off" << std::endl;
        }

        mithril::Coordinator c(config);
        c.Run();
    } catch (const std::exception& e) {
        spdlog::error("fatal exception: {}", e.what());
        return 1;
    }

    mithril::http::ApplicationResolver.Reset(nullptr);
    mithril::http::DeinitializeSSL();

    return 0;
}
