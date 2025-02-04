add_library(common STATIC
    src/http/Connection.cpp
    src/http/ParsedUrl.cpp
    src/http/Request.cpp
    src/http/RequestExecutor.cpp
)

target_include_directories(common PUBLIC include)
