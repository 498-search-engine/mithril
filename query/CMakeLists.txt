add_library(query STATIC
    src/Lexer.cpp
    src/intersect.cpp
    src/network.cpp
)
target_include_directories(query PUBLIC src)
target_link_libraries(query PUBLIC
    spdlog::spdlog
    lib::core
    common
)

add_executable(mithril_lexer
    src/main.cpp
)
target_link_libraries(mithril_lexer PRIVATE
    spdlog::spdlog
    query
)


add_executable(mithril_child
    src/mithril_child.cpp
)
target_link_libraries(mithril_child PRIVATE
    spdlog::spdlog
    query
)

# Add test executable
add_executable(test_lexer
    tests/test_lexer.cpp
)
target_link_libraries(test_lexer PRIVATE
    query
    GTest::gtest
    GTest::gtest_main
)

# Add test executable
add_executable(test_query
    tests/test_query.cpp
)
target_link_libraries(test_query PRIVATE
    query
    GTest::gtest
    GTest::gtest_main
    index
)

# Add test executable
add_executable(parser_driver
    tests/parser_driver.cpp
)
target_link_libraries(parser_driver PRIVATE
    query
    index
)



# Add lexer driver executable
add_executable(lexer_driver
    tests/lexer_driver.cpp
)
target_link_libraries(lexer_driver PRIVATE
    query
)


# Add query driver executable
add_executable(query_driver
    tests/query_driver.cpp
)
target_link_libraries(query_driver PRIVATE
    query
    index
)

# Add query driver executable
add_executable(and_test
    tests/and_test.cpp
)
target_link_libraries(and_test PRIVATE
    query
    index
    common
)

# Add query driver executable
add_executable(or_test
    tests/or_test.cpp
)
target_link_libraries(or_test PRIVATE
    query
    index
    common
)



# Register the test
add_test(NAME LexerTest COMMAND test_lexer)

# add_executable(index_debug
#     src/debugPrintIndex.cpp
# )
# target_link_libraries(index_debug PRIVATE
#     index
# )

# add_executable(test_docReader
#     src/test_docReader.cpp
# )
# target_link_libraries(test_docReader PRIVATE
#     index
# )

# add_executable(test_termReader
#     src/test_termReader.cpp
# )
# target_link_libraries(test_termReader PRIVATE
#     index
# )

# add_executable(test_termOR src/test_termOR.cpp)
# target_link_libraries(test_termOR PRIVATE index)

# add_executable(test_termAND src/test_termAND.cpp)
# target_link_libraries(test_termAND PRIVATE index)
