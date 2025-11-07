# Test configuration
# Using doctest header-only library

# Download doctest if not present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doctest.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/doctest.h"
        SHOW_PROGRESS
    )
endif()

# Test executable
add_executable(market_data_tests
    test_circular_buffer.cpp
    test_mpmc_queue.cpp
    test_memory_pool.cpp
    test_main.cpp
)

target_include_directories(market_data_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(market_data_tests
    PRIVATE
        market_data_core
)

# Add test
add_test(NAME market_data_tests COMMAND market_data_tests)
