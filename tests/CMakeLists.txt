enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

file(COPY config DESTINATION ${CMAKE_BINARY_DIR}/tests)

set(TEST_SOURCES
    src/HtmlEntity.cpp
    src/Robots.cpp
    src/Serialization.cpp
    src/StringTrie.cpp
    src/URL.cpp
)

add_executable(mithril_tests ${TEST_SOURCES})
target_link_libraries(mithril_tests PRIVATE
    gtest_main
    lib::core
    common
    crawler
)

add_test(NAME mithril_tests COMMAND mithril_tests)


add_executable(crawler_rank_test 
    src/CrawlerRankerTest.cpp
)

add_executable(crawler_rank_struct_test 
    src/CrawlerRankerStructTest.cpp
)

target_link_libraries(crawler_rank_test PRIVATE
    lib::core
    common
)

target_link_libraries(crawler_rank_struct_test PRIVATE
    lib::core
    common
)