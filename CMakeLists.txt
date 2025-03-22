cmake_minimum_required(VERSION 3.25)
project(Mithril VERSION 0.1.0
        DESCRIPTION "a search engine system"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(
    $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
    $<$<NOT:$<CONFIG:Debug>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
)

# Add subdirectories
add_subdirectory(external)
add_subdirectory(lib)
add_subdirectory(common)
add_subdirectory(crawler)
add_subdirectory(index)
add_subdirectory(ranking)
add_subdirectory(tests)
