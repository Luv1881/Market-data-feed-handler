# Benchmark configuration
# Using Google Benchmark library (if available)

# For now, create a simple manual benchmark
add_executable(market_data_benchmarks
    benchmark_circular_buffer.cpp
    benchmark_latency.cpp
)

target_link_libraries(market_data_benchmarks
    PRIVATE
        market_data_core
)
