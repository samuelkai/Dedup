cmake_minimum_required(VERSION 3.10)
project(dedup LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(TEST_DIR ${PROJECT_SOURCE_DIR}/test)

# A library for xxHash
#-------------------------------------------------
add_library(xxH
    ${SOURCE_DIR}/xxhash.c
)

target_include_directories(xxH PRIVATE
    ${INCLUDE_DIR}
)

# A library for source files other than main
#-------------------------------------------------
add_library(GatherHashes
    ${SOURCE_DIR}/gather_hashes.cpp
    ${SOURCE_DIR}/gather_hashes.h
)

target_include_directories(GatherHashes PRIVATE
    ${INCLUDE_DIR}
)

target_link_libraries(GatherHashes
    stdc++fs
    xxH
)
#-------------------------------------------------


# Main executable
#-------------------------------------------------
add_executable(${PROJECT_NAME}
    ${SOURCE_DIR}/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    stdc++fs
    GatherHashes
)
#-------------------------------------------------


# Catch library
#-------------------------------------------------
add_library(Catch INTERFACE)

target_include_directories(Catch INTERFACE
    ${INCLUDE_DIR}/catch2
)
#-------------------------------------------------


# Catch's main as a separate object file, so it
# doesn't have to be recompiled
#-------------------------------------------------

add_library(test_main OBJECT
    ${TEST_DIR}/test_main.cpp
)
#-------------------------------------------------


# The actual unit tests
#-------------------------------------------------
add_executable(unit_tests
    ${TEST_DIR}/unit_tests.cpp
    $<TARGET_OBJECTS:test_main>
)

target_include_directories(unit_tests PRIVATE
    ${SOURCE_DIR}
)

target_link_libraries(unit_tests
    Catch
    GatherHashes
)

enable_testing()

add_test(test_all unit_tests)
#-------------------------------------------------
