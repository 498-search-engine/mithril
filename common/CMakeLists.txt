find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_library(common STATIC
    src/data/Gzip.cpp
    src/data/Reader.cpp
    src/data/Writer.cpp

    src/html/Entity.cpp
    src/html/Link.cpp
    src/html/Parser.cpp
    src/html/Tags.cpp

    src/http/AsyncResolver.cpp
    src/http/Connection.cpp
    src/http/Request.cpp
    src/http/RequestExecutor.cpp
    src/http/Resolver.cpp
    src/http/Response.cpp
    src/http/SSL.cpp
    src/http/SyncResolver.cpp
    src/http/URL.cpp

    src/metrics/Metrics.cpp
    src/metrics/MetricsServer.cpp

    src/ranking/CrawlerRanker.cpp
    src/ranking/PageRankReader.cpp

    src/Clock.cpp
    src/FileSystem.cpp
    src/StringTrie.cpp
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