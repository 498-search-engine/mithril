# Check if we're not using Apple Clang on macOS
# since it is not well supported on Apple Clang and causes issues.
execute_process(COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE COMPILER_VERSION)
if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND COMPILER_VERSION MATCHES "Apple" AND CMAKE_SYSTEM_NAME MATCHES "Darwin"))
    find_package(OpenMP REQUIRED)
else()
    # Define an empty INTERFACE target (no actual linking)
    if(NOT TARGET OpenMP::OpenMP_CXX)
        add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
    endif()
endif()

add_library(pagerank STATIC
    src/pagerank.cpp
)

target_link_libraries(pagerank PUBLIC
    spdlog::spdlog
    lib::core
    common
    OpenMP::OpenMP_CXX
)

# Only apply `-fopenmp` on non-Apple Clang systems
if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME MATCHES "Darwin"))
    target_compile_options(pagerank PRIVATE -fopenmp)
endif()

add_executable(pagerank_bench
    src/pagerank_bench.cpp
)

add_executable(pagerank_forwardlinks
    src/pagerank_forwardlinks.cpp
)

add_executable(domainrank_forwardlinks
    src/domainrank_forwardlinks.cpp
)

file(COPY config DESTINATION ${CMAKE_BINARY_DIR}/ranking)

target_link_libraries(pagerank_bench PRIVATE
    pagerank
)

target_link_libraries(pagerank_forwardlinks PRIVATE
    pagerank
)

target_link_libraries(domainrank_forwardlinks PRIVATE
    pagerank
)
