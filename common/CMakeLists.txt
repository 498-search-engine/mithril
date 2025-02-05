find_package(OpenSSL REQUIRED)

add_library(common STATIC
    src/html/Parser.cpp
    src/html/Tags.cpp

    src/http/Connection.cpp
    src/http/ParsedUrl.cpp
    src/http/Request.cpp
    src/http/RequestExecutor.cpp
    src/http/Response.cpp
    src/http/SSL.cpp
)

target_include_directories(common PUBLIC include)

target_link_libraries(common OpenSSL::SSL OpenSSL::Crypto)
