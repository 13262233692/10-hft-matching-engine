cmake_minimum_required(VERSION 3.20)
project(HFTMatchingEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/O2 /W4 /permissive-)
    add_definitions(-D_WIN32_WINNT=0x0A00)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/core/Order.cpp
    src/core/FIXParser.cpp
    src/core/OrderBook.cpp
    src/core/RedBlackTree.cpp
    src/core/OrderQueue.cpp
    src/net/TCPServer.cpp
    src/net/WebSocketServer.cpp
)

add_executable(hft_engine ${SOURCES})

if(WIN32)
    target_link_libraries(hft_engine PRIVATE ws2_32 mswsock)
endif()

set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_order_book.cpp
    tests/test_fix_parser.cpp
    tests/test_performance.cpp
    src/core/Order.cpp
    src/core/FIXParser.cpp
    src/core/OrderBook.cpp
    src/core/RedBlackTree.cpp
    src/core/OrderQueue.cpp
)

add_executable(hft_tests ${TEST_SOURCES})

if(WIN32)
    target_link_libraries(hft_tests PRIVATE ws2_32 mswsock)
endif()
