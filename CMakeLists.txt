cmake_minimum_required(VERSION 3.15)
project(bloom_valuation VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BLOOM_VALUATION_BUILD_TESTS "Build tests" ON)

include(GNUInstallDirs)

add_subdirectory(lib)
add_subdirectory(src)

if(BLOOM_VALUATION_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
