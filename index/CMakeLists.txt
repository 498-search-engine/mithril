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
    src/TermPhrase.cpp
    src/TermQuote.cpp
    src/PositionIndex.cpp
    src/GenericTermReader.cpp
)
target_include_directories(index PUBLIC src)
target_link_libraries(index PUBLIC
    spdlog::spdlog
    lib::core
    common
)

# Main application 
add_executable_with_copy(mithril_indexer src/main.cpp)
target_link_libraries(mithril_indexer PRIVATE index)

# Test executables
add_executable(test_docReader tests/test_docReader.cpp)
add_executable(test_termReader tests/test_termReader.cpp)
add_executable(test_termOR tests/test_termOR.cpp)
add_executable(test_termAND tests/test_termAND.cpp)
add_executable(test_termQuote tests/test_termQuote.cpp)
add_executable(test_termPhrase tests/test_termPhrase.cpp)
add_executable(test_genericTermReader tests/test_genericTermReader.cpp)

# Test targets linking
target_link_libraries(test_docReader PRIVATE index)
target_link_libraries(test_termReader PRIVATE index)
target_link_libraries(test_termOR PRIVATE index)
target_link_libraries(test_termAND PRIVATE index)
target_link_libraries(test_termQuote PRIVATE index)
target_link_libraries(test_termPhrase PRIVATE index)
target_link_libraries(test_genericTermReader PRIVATE index)


add_executable_with_copy(generate_position_sync_points src/generate_position_sync_points.cpp)
target_link_libraries(generate_position_sync_points PRIVATE index)

# add_executable(index_debug
#     src/debugPrintIndex.cpp
# )
# target_link_libraries(index_debug PRIVATE index)