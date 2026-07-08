cmake_minimum_required(VERSION 3.14)
project(matching-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if (ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# 本地交互版引擎（保留原来的）
add_executable(engine
    src/main.cpp
    src/OrderBook.cpp
)
target_include_directories(engine PRIVATE include)

# TCP 服务器
add_executable(server
    src/server_main.cpp
    src/Server.cpp
    src/OrderBook.cpp
)
target_include_directories(server PRIVATE include)
target_link_libraries(server pthread)

# TCP 客户端
add_executable(client
    src/client.cpp
)
target_include_directories(client PRIVATE include)
target_link_libraries(client pthread)

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

add_executable(server_tests
    tests/test_server.cpp
    src/Server.cpp
    src/OrderBook.cpp
)
target_include_directories(server_tests PRIVATE include)
target_link_libraries(server_tests GTest::gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(server_tests DISCOVERY_MODE PRE_TEST)
