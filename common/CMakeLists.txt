find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_library(common STATIC
    src/data/Reader.cpp
    src/data/Writer.cpp

    src/html/Link.cpp
    src/html/Parser.cpp
    src/html/Tags.cpp

    src/http/Connection.cpp
    src/http/Request.cpp
    src/http/RequestExecutor.cpp
    src/http/Response.cpp
    src/http/SSL.cpp
    src/http/URL.cpp

    src/ranking/CrawlerRanker.cpp

    src/Clock.cpp
    src/FileSystem.cpp
    src/ThreadSync.cpp
    src/Util.cpp
)

target_include_directories(common PUBLIC include)

target_link_libraries(common
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    spdlog::spdlog
    lib::core
)
