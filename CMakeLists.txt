cmake_minimum_required(VERSION 3.25)
project(Mithril VERSION 0.1.0
        DESCRIPTION "a search engine system"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(
    $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
    $<$<NOT:$<CONFIG:Debug>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
)

option(USE_JEMALLOC "Use jemalloc as memory allocator" OFF)

if(USE_JEMALLOC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JEMALLOC jemalloc)

    if(JEMALLOC_FOUND)
        message(STATUS "Using jemalloc allocator")

        # Set compile definition to indicate jemalloc is available
        add_compile_definitions(USE_JEMALLOC)

        # Add the linker flag globally
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ljemalloc")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -ljemalloc")

        # Add include directories globally
        include_directories(${JEMALLOC_INCLUDE_DIRS})
    else()
        message(WARNING "jemalloc requested but not found. Using standard allocator.")
    endif()
endif()

# Add subdirectories
add_subdirectory(external)
add_subdirectory(lib)
add_subdirectory(common)
add_subdirectory(crawler)
add_subdirectory(index)
add_subdirectory(ranking)
add_subdirectory(web)
add_subdirectory(tests)
