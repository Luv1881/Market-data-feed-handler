# Examples

add_executable(simple_example
    simple_example.cpp
)

target_link_libraries(simple_example
    PRIVATE
        market_data_core
)
