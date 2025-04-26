add_library(web STATIC
    src/Server.cpp
    src/SearchPlugin.cpp
)
target_include_directories(web PUBLIC src)
target_link_libraries(web PUBLIC
    query
    pthread
)

add_executable_with_copy(mithril_server src/main.cpp)
target_link_libraries(mithril_server web)

file(COPY frontend DESTINATION ${CMAKE_SOURCE_DIR}/bin)