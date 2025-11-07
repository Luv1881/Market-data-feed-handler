#include "market_data/lockfree/circular_buffer.hpp"
#include "market_data/core/market_event.hpp"
#include "market_data/utils/timestamp.hpp"
#include <iostream>
#include <iomanip>

using namespace market_data;

void benchmark_circular_buffer_push_pop() {
    constexpr std::size_t ITERATIONS = 10'000'000;
    CircularBuffer<MarketEvent, 1024 * 1024> buffer;

    MarketEvent event;
    event.venue_id = 1;
    event.sequence_number = 0;
    event.event_type = EventType::TRADE;

    Timestamp::initialize();

    std::cout << "Benchmarking CircularBuffer push/pop..." << std::endl;
    std::cout << "Iterations: " << ITERATIONS << std::endl;

    uint64_t start_tsc = rdtscp();

    for (std::size_t i = 0; i < ITERATIONS; ++i) {
        event.sequence_number = i;
        buffer.try_push(event);

        MarketEvent received;
        buffer.try_pop(received);
    }

    uint64_t end_tsc = rdtscp();
    uint64_t total_cycles = end_tsc - start_tsc;
    uint64_t total_ns = Timestamp::tsc_to_ns(total_cycles);

    double cycles_per_op = static_cast<double>(total_cycles) / (ITERATIONS * 2); // push + pop
    double ns_per_op = static_cast<double>(total_ns) / (ITERATIONS * 2);
    double ops_per_sec = (ITERATIONS * 2.0) / (total_ns / 1e9);

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Cycles per operation: " << std::fixed << std::setprecision(2)
              << cycles_per_op << std::endl;
    std::cout << "  Nanoseconds per operation: " << std::fixed << std::setprecision(2)
              << ns_per_op << " ns" << std::endl;
    std::cout << "  Operations per second: " << std::fixed << std::setprecision(0)
              << ops_per_sec << std::endl;
}

int main() {
    std::cout << "=== Market Data Handler Benchmarks ===" << std::endl;
    benchmark_circular_buffer_push_pop();
    return 0;
}
