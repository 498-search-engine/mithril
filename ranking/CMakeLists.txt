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

target_link_libraries(pagerank_bench PRIVATE
    spdlog::spdlog
    lib::core
    common
    pagerank
    OpenMP::OpenMP_CXX
)
