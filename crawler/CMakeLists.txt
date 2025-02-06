find_package(OpenSSL REQUIRED)

add_executable(crawler
    src/Coordinator.cpp
    src/DocumentQueue.cpp
    src/RequestManager.cpp
    src/UrlFrontier.cpp
    src/Worker.cpp

    src/main.cpp
)

target_include_directories(crawler PRIVATE src)

target_link_libraries(crawler PRIVATE OpenSSL::SSL OpenSSL::Crypto common)
