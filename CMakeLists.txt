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

# Find all packages FIRST before using them
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(websocketpp REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)

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

# Gather source files (exclude main.cc)
file(GLOB_RECURSE LIB_SOURCES "src/*.cc")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cc$")

# Create a library with your core logic
add_library(${PROJECT_NAME}_lib STATIC ${LIB_SOURCES})

target_include_directories(${PROJECT_NAME}_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${WEBSOCKETPP_INCLUDE_DIR}
)

if(ASIO_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME}_lib PUBLIC ${ASIO_INCLUDE_DIR})
    target_compile_definitions(${PROJECT_NAME}_lib PUBLIC
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_THREAD_
    )
endif()

target_link_libraries(${PROJECT_NAME}_lib PUBLIC
    Threads::Threads
    Boost::system
    Boost::thread
    spdlog::spdlog
)

# Main executable (just links to the library)
add_executable(${PROJECT_NAME} src/main.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Testing
enable_testing()
add_executable(tests tests/protocol_tests.cc)

target_link_libraries(tests PRIVATE
    ${PROJECT_NAME}_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)
