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

add_library(ranking STATIC 
    src/PageRank.cpp
    src/StaticRanker.cpp
)
target_include_directories(ranking PUBLIC src)
target_link_libraries(ranking PUBLIC
    spdlog::spdlog
    lib::core
    common
    OpenMP::OpenMP_CXX
)

# Only apply `-fopenmp` on non-Apple Clang systems
if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND COMPILER_VERSION MATCHES "Apple" AND CMAKE_SYSTEM_NAME MATCHES "Darwin"))
    target_compile_options(ranking PRIVATE -fopenmp)
endif()

# Handle tests folder

# PageRank related
add_executable(pagerank_bench tests/PageRank_Bench.cpp)
target_include_directories(pagerank_bench PRIVATE src)
target_link_libraries(pagerank_bench PRIVATE ranking)

add_executable(pagerank_sim tests/PageRank_Sim.cpp)
target_include_directories(pagerank_sim PRIVATE src)
target_link_libraries(pagerank_sim PRIVATE ranking)

add_executable(pagerank_reader tests/PageRank_ReaderSim.cpp)
target_include_directories(pagerank_reader PRIVATE src)
target_link_libraries(pagerank_reader PRIVATE ranking)

add_executable(domainrank_sim tests/DomainRank_Sim.cpp)
target_include_directories(domainrank_sim PRIVATE src)
target_link_libraries(domainrank_sim PRIVATE ranking)

# Crawler ranker related
add_executable(crawler_rank_test tests/CrawlerURLRanker_Sim.cpp)
target_link_libraries(crawler_rank_test PRIVATE common)

add_executable(crawler_rank_struct_test tests/CrawlerURLRanker_StructSim.cpp)
target_link_libraries(crawler_rank_struct_test PRIVATE common)

# Static ranker related
add_executable(static_rank_test tests/StaticURLRanker_Sim.cpp)
target_link_libraries(static_rank_test PRIVATE ranking)

add_executable(static_rank_struct_test tests/StaticURLRanker_StructSim.cpp)
target_link_libraries(static_rank_struct_test PRIVATE ranking)

file(COPY config DESTINATION ${CMAKE_BINARY_DIR}/ranking)