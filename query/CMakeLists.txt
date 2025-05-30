add_library(query STATIC
    src/Lexer.cpp
    src/intersect.cpp
    src/network.cpp
    src/QueryCoordinator.cpp
    src/QueryManager.cpp
)
target_include_directories(query PUBLIC 
    src
    ${CMAKE_SOURCE_DIR}/ranking/src
)
target_link_libraries(query PUBLIC
    index
    ranking
)

# Main applications
add_executable_with_copy(mithril_lexer src/main.cpp)
add_executable_with_copy(mithril_coordinator src/mithril_coordinator.cpp)
add_executable_with_copy(mithril_worker src/mithril_worker.cpp)
add_executable_with_copy(mithril_manager src/mithril_manager.cpp)

target_link_libraries(mithril_lexer PRIVATE query)
target_link_libraries(mithril_coordinator PRIVATE query)
target_link_libraries(mithril_worker PRIVATE query)
target_link_libraries(mithril_manager PRIVATE query)

# Test executables
set(TEST_LIBS
    query
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
add_executable(quote_test tests/quote_test.cpp)
add_executable(test_freq_ct tests/lexer_token_freq_test.cpp)

# Test targets linking
target_link_libraries(test_lexer PRIVATE ${TEST_LIBS})
target_link_libraries(test_query PRIVATE ${TEST_LIBS})
target_link_libraries(parser_driver PRIVATE query)
target_link_libraries(manager_driver PRIVATE query)
target_link_libraries(lexer_driver PRIVATE query)
target_link_libraries(query_driver PRIVATE query)
target_link_libraries(and_test PRIVATE query)
target_link_libraries(or_test PRIVATE query)
target_link_libraries(quote_test PRIVATE query)
target_link_libraries(test_freq_ct PRIVATE query)

# Tests registration
add_test(NAME LexerTest COMMAND test_lexer)

file(COPY servers.conf DESTINATION ${CMAKE_SOURCE_DIR}/bin)
file(COPY mithril_manager.conf DESTINATION ${CMAKE_SOURCE_DIR}/bin)
file(COPY mithril_manager.conf DESTINATION ${DESTINATION_DIR})