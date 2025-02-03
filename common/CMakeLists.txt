add_library(common STATIC
    src/http/RequestExecutor.cpp
)

target_include_directories(common PUBLIC include)
