# Benchmark configuration
# Using Google Benchmark library (if available)

# Create separate executables for each benchmark
add_executable(benchmark_circular_buffer
    benchmark_circular_buffer.cpp
)

target_link_libraries(benchmark_circular_buffer
    PRIVATE
        market_data_core
)

add_executable(benchmark_latency
    benchmark_latency.cpp
)

target_link_libraries(benchmark_latency
    PRIVATE
        market_data_core
)
