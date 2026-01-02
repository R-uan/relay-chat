cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME relay_chat)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
project(${PROJECT_NAME} CXX)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode enabled")
    add_compile_options(-g -O0 -Wall -Wextra)
    
    # Optional: Enable sanitizers in debug mode
    option(ENABLE_ASAN "Enable AddressSanitizer" ON)
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Find Boost (with required components for websocketpp)
find_package(Boost REQUIRED COMPONENTS system thread)

# Find websocketpp
find_package(websocketpp REQUIRED)

# Find spdlog
find_package(spdlog REQUIRED)

# Find GTest
find_package(GTest REQUIRED)

# Find Threads
find_package(Threads REQUIRED)

# Optionally find asio if using standalone
find_path(ASIO_INCLUDE_DIR 
    NAMES asio.hpp
)

find_path(WEBSOCKETPP_INCLUDE_DIR 
    NAMES websocketpp/server.hpp
    REQUIRED
)

message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
if(ASIO_INCLUDE_DIR)
    message(STATUS "Found asio: ${ASIO_INCLUDE_DIR}")
endif()
message(STATUS "Found websocketpp: ${WEBSOCKETPP_INCLUDE_DIR}")

# Gather source files
file(GLOB_RECURSE SOURCES "src/*.cc")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${WEBSOCKETPP_INCLUDE_DIR}
)

# Add asio include if found (for standalone mode)
if(ASIO_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_THREAD_
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    Boost::system
    Boost::thread
    spdlog::spdlog
)
# Testing
# enable_testing()

# add_executable(
#relay_chat_tests
# tests/client_tests.cc
#)
#target_link_libraries(
#  relay_chat_tests
#  GTest::gtest_main
#)
#include(GoogleTest)
#gtest_discover_tests(relay_chat_tests)
