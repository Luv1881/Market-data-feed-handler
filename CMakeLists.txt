cmake_minimum_required(VERSION 3.25)
project(market-data-handler VERSION 1.0.0 LANGUAGES CXX)

# C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -ffast-math -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer")

# Common flags for all builds
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-interference-size  # Suppress C++20 std::hardware_destructive_interference_size ABI warning
    -Wno-unused-result      # Allow ignoring [[nodiscard]] in test/benchmark code
    -fno-exceptions
    -fno-rtti
    $<$<CONFIG:Release>:-fno-omit-frame-pointer>
)

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(_GNU_SOURCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

# Find packages
find_package(Threads REQUIRED)

# Option to enable/disable sanitizers
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# Core library
add_library(market_data_core STATIC
    src/core/market_event.cpp
    src/memory/memory_pool.cpp
    src/threading/thread_utils.cpp
    src/metrics/latency_tracker.cpp
    src/utils/timestamp.cpp
)

target_link_libraries(market_data_core
    PUBLIC
        Threads::Threads
)

# Main executable
add_executable(market_data_handler
    src/main.cpp
)

target_link_libraries(market_data_handler
    PRIVATE
        market_data_core
)

# Tests (optional)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks (optional)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(TARGETS market_data_handler market_data_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/market_data
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)
