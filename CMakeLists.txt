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

# Setup bin pipeline
set(DESTINATION_DIR "${CMAKE_BINARY_DIR}/../bin")
file(MAKE_DIRECTORY ${DESTINATION_DIR})
function(add_executable_with_copy target)
    add_executable(${target} ${ARGN})
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${target}>
            ${DESTINATION_DIR}
        COMMENT "Copying ${target} to ${DESTINATION_DIR}"
    )
endfunction()

# Add subdirectories
add_subdirectory(external)
add_subdirectory(lib)
add_subdirectory(common)
add_subdirectory(crawler)
add_subdirectory(index)
add_subdirectory(ranking)
add_subdirectory(web)
add_subdirectory(tests)