find_package(OpenMP REQUIRED)

add_library(pagerank STATIC
    src/pagerank.cpp
)

target_link_libraries(pagerank PUBLIC
    spdlog::spdlog
    lib::core
    common
    OpenMP::OpenMP_CXX
)
target_compile_options(pagerank PRIVATE -fopenmp)

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
    spdlog::spdlog
    lib::core
    common
    pagerank
    OpenMP::OpenMP_CXX
)

target_link_libraries(pagerank_forwardlinks PRIVATE
    spdlog::spdlog
    lib::core
    common
    pagerank
    OpenMP::OpenMP_CXX
)

target_link_libraries(domainrank_forwardlinks PRIVATE
    spdlog::spdlog
    lib::core
    common
    pagerank
    OpenMP::OpenMP_CXX
)
