cmake_minimum_required(VERSION 3.14)
project(matching-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 主引擎可执行文件
add_executable(engine
    src/main.cpp
    src/OrderBook.cpp
)
target_include_directories(engine PRIVATE include)

# ---- 测试 ----
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(tests
    tests/test_orderbook.cpp
    src/OrderBook.cpp
)
target_include_directories(tests PRIVATE include)
target_link_libraries(tests GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)
