add_library(index STATIC
    src/TermStore.cpp
    src/InvertedIndex.cpp
    src/PostingBlock.cpp
    src/DocumentMapReader.cpp
    src/TermReader.cpp
    src/TermDictionary.cpp
    src/IndexStreamReader.cpp
    src/TermOR.cpp
    src/TermAND.cpp
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

add_executable(test_termReader
    src/test_termReader.cpp
)
target_link_libraries(test_termReader PRIVATE
    index
)

add_executable(test_termOR src/test_termOR.cpp)
target_link_libraries(test_termOR PRIVATE index)

add_executable(test_termAND src/test_termAND.cpp)
target_link_libraries(test_termAND PRIVATE index)
