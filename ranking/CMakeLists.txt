find_package(OpenMP REQUIRED)

add_library(ranking STATIC
    src/pagerank.cpp
)

target_link_libraries(ranking PUBLIC
    spdlog::spdlog
    lib::core
    common
    omp
)
target_compile_options(ranking PRIVATE -fopenmp)
