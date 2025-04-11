add_library(query STATIC
    src/Lexer.cpp
    src/intersect.cpp
    src/network.cpp
    src/QueryCoordinator.cpp
    src/QueryManager.cpp
)
target_include_directories(query PUBLIC src)
target_link_libraries(query PUBLIC
    spdlog::spdlog
    lib::core
    common
)

# Main applications
add_executable_with_copy(mithril_lexer src/main.cpp)
add_executable_with_copy(mithril_coordinator src/mithril_coordinator.cpp)
add_executable_with_copy(mithril_worker src/mithril_worker.cpp)

target_link_libraries(mithril_lexer PRIVATE query)
target_link_libraries(mithril_coordinator PRIVATE query)
target_link_libraries(mithril_worker PRIVATE query index)

# Test executables
set(TEST_LIBS
    query
    GTest::gtest
    GTest::gtest_main
)

add_executable(test_lexer tests/test_lexer.cpp)
add_executable(test_query tests/test_query.cpp)
add_executable(parser_driver tests/parser_driver.cpp)
add_executable(manager_driver tests/manager_driver.cpp)
add_executable(lexer_driver tests/lexer_driver.cpp)
add_executable(query_driver tests/query_driver.cpp)
add_executable(and_test tests/and_test.cpp)
add_executable(or_test tests/or_test.cpp)

# Test targets linking
target_link_libraries(test_lexer PRIVATE ${TEST_LIBS})
target_link_libraries(test_query PRIVATE ${TEST_LIBS} index)
target_link_libraries(parser_driver PRIVATE query index)
target_link_libraries(manager_driver PRIVATE query index)
target_link_libraries(lexer_driver PRIVATE query)
target_link_libraries(query_driver PRIVATE query index)
target_link_libraries(and_test PRIVATE query index common)
target_link_libraries(or_test PRIVATE query index common)

# Tests registration
add_test(NAME LexerTest COMMAND test_lexer)