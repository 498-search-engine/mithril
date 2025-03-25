find_package(OpenMP REQUIRED)

add_library(pagerank_bench STATIC
    src/pagerank.cpp
    src/pagerank_bench.cpp
)

target_link_libraries(pagerank_bench PUBLIC
    spdlog::spdlog
    lib::core
    common
    omp
)
target_compile_options(pagerank_bench PRIVATE -fopenmp)
