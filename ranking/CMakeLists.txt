add_library(pagerank STATIC
    src/pagerank.cpp
)

target_include_directories(common PUBLIC include)
