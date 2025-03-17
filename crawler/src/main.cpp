#include "Config.h"
#include "Coordinator.h"
#include "core/memory.h"
#include "http/AsyncResolver.h"
#include "http/Resolver.h"
#include "http/SSL.h"

#include <csignal>
#include <exception>
#include <string>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
#if !defined(NDEBUG)
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    signal(SIGPIPE, SIG_IGN);

    mithril::http::InitializeSSL();
    mithril::http::ApplicationResolver = core::UniquePtr<mithril::http::Resolver>(new mithril::http::AsyncResolver{});

    try {
        auto config = mithril::LoadConfigFromFile(argc > 1 ? argv[1] : "crawler.conf");
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
