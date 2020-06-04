cmake_minimum_required(VERSION 3.13)
project(dedup LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)
set(TEST_DIR ${PROJECT_SOURCE_DIR}/test)

add_compile_options(-Wall -Wextra -pedantic -Werror -Wshadow)

# Checks
#-------------------------------------------------
# set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*,-fuchsia-default-arguments;-header-filter=.;")
# add_compile_options(-fsanitize=address,undefined)
# add_link_options(-fsanitize=address,undefined)
#-------------------------------------------------


# A library for source files other than main
# (to be linked to main and tests)
#-------------------------------------------------
add_library(Others
    ${SOURCE_DIR}/parse.cpp
    ${SOURCE_DIR}/find_duplicates.cpp
    ${SOURCE_DIR}/deal_with_duplicates.cpp
    ${SOURCE_DIR}/utilities.cpp
)

target_include_directories(Others PRIVATE
    ${THIRD_PARTY_DIR}
)

target_link_libraries(Others
    stdc++fs
)
#-------------------------------------------------


# Main executable
#-------------------------------------------------
add_executable(${PROJECT_NAME}
    ${SOURCE_DIR}/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${THIRD_PARTY_DIR}
)

target_link_libraries(${PROJECT_NAME}
    stdc++fs
    Others
)
#-------------------------------------------------


# Catch library
#-------------------------------------------------
add_library(Catch INTERFACE)

target_include_directories(Catch INTERFACE
    ${THIRD_PARTY_DIR}/catch2
)
#-------------------------------------------------


# Catch's main as a separate object file, so it
# doesn't have to be recompiled
#-------------------------------------------------

add_library(test_main OBJECT
    ${TEST_DIR}/test_main.cpp
)

target_include_directories(test_main PRIVATE
    ${THIRD_PARTY_DIR}
)
#-------------------------------------------------


# The actual tests
#-------------------------------------------------
add_executable(tests
    ${TEST_DIR}/tests.cpp
    $<TARGET_OBJECTS:test_main>
)

target_include_directories(tests PRIVATE
    ${SOURCE_DIR}
    ${THIRD_PARTY_DIR}
)

target_link_libraries(tests
    Catch
    Others
)

enable_testing()

add_test(test_all tests)
#-------------------------------------------------
