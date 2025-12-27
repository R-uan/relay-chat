cmake_minimum_required(VERSION 3.11)

set(PROJECT_NAME relay_chat)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

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

include(FetchContent)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG 32dd298dc2d60fe0454e70b818d64941392c5b41
)

FetchContent_MakeAvailable(spdlog)

project(${PROJECT_NAME} CXX)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
