cmake_minimum_required(VERSION 3.11) # for fetchcontent

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(json)

add_library(web STATIC
    src/Server.cpp
    src/SearchPlugin.cpp
)
target_include_directories(web PUBLIC src)
target_link_libraries(web PUBLIC
    spdlog::spdlog
    lib::core
    common
    pthread
    nlohmann_json::nlohmann_json
)

add_executable(mithril_server
    src/main.cpp
)
target_link_libraries(mithril_server
    web
    pthread
)
