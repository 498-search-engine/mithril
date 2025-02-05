find_package(OpenSSL REQUIRED)

add_executable(crawler
    src/main.cpp
)

target_include_directories(crawler PRIVATE src)

target_link_libraries(crawler PRIVATE OpenSSL::SSL OpenSSL::Crypto common)
