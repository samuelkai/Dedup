cmake_minimum_required(VERSION 3.10)
project(dedup LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/gather_hashes.cpp
    src/MurmurHash3.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} stdc++fs)

add_subdirectory(include/catch2)
enable_testing()

add_executable(unit_tests test/testi.cpp)
target_link_libraries(unit_tests
    Catch2::Test
)

add_test(test_all unit_tests)