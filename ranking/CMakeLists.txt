find_package(OpenMP REQUIRED)

set(OpenMP_C_FLAGS "-fopenmp=libomp")
set(OpenMP_CXX_FLAGS "-fopenmp=libomp")
set(OpenMP_EXE_LINKER_FLAGS "-fopenmp=libomp")

add_library(ranking STATIC
    src/pagerank.cpp
)

target_link_libraries(ranking PUBLIC
    spdlog::spdlog
    lib::core
    common
)
target_compile_options(ranking PRIVATE ${OpenMP_CXX_FLAGS})
target_include_directories(ranking PUBLIC include)