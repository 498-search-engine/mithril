add_library(web STATIC
    src/Server.cpp
)
target_include_directories(web PUBLIC src)
target_link_libraries(web PUBLIC
    spdlog::spdlog
    lib::core
    common
    pthread
)

add_executable(mithril_server
    src/main.cpp
)
target_link_libraries(mithril_server
    web
    pthread
)
