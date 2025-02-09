find_package(OpenSSL REQUIRED)

add_executable(crawler
    src/Coordinator.cpp
    src/DocumentQueue.cpp
    src/RequestManager.cpp
    src/UrlFrontier.cpp
    src/UrlSet.cpp
    src/Worker.cpp
    src/config.cpp
    src/main.cpp
)

target_include_directories(crawler PRIVATE src)

target_link_libraries(crawler PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto

    lib::core
    common
)
