find_package(OpenSSL REQUIRED)

add_library(crawler STATIC
    src/Coordinator.cpp
    src/DocumentQueue.cpp
    src/HostRateLimiter.cpp
    src/MiddleQueue.cpp
    src/RequestManager.cpp
    src/Robots.cpp
    src/UrlFrontier.cpp
    src/Worker.cpp
    src/Config.cpp
)

target_include_directories(crawler PUBLIC src)

target_link_libraries(crawler PUBLIC
    spdlog::spdlog
    lib::core
    common
)

add_executable(mithril_crawler
    src/main.cpp
)

target_link_libraries(mithril_crawler PRIVATE
    crawler
)
