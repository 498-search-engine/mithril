add_library(index STATIC
    src/TermStore.cpp
    src/InvertedIndex.cpp
    src/PostingBlock.cpp
    src/DocumentMapReader.cpp
)
target_include_directories(index PUBLIC src)
target_link_libraries(index PUBLIC
    spdlog::spdlog
    lib::core
    common
)

add_executable(mithril_indexer
    src/main.cpp
)
target_link_libraries(mithril_indexer PRIVATE
    spdlog::spdlog
    index
)

add_executable(index_debug
    src/debugPrintIndex.cpp
)
target_link_libraries(index_debug PRIVATE
    index
)

add_executable(test_docReader
    src/test_docReader.cpp
)
target_link_libraries(test_docReader PRIVATE
    index
)