#include "market_data/metrics/latency_tracker.hpp"
#include "market_data/utils/timestamp.hpp"
#include <iostream>
#include <iomanip>

using namespace market_data;

void benchmark_latency_histogram() {
    LatencyHistogram histogram;
    Timestamp::initialize();

    std::cout << "\nBenchmarking LatencyHistogram..." << std::endl;

    // Record some samples
    constexpr std::size_t SAMPLES = 1'000'000;

    uint64_t start_tsc = rdtscp();

    for (std::size_t i = 0; i < SAMPLES; ++i) {
        // Simulate various latencies (1-100 microseconds)
        uint64_t latency_ns = 1000 + (i % 100) * 1000; // 1-100 us
        histogram.record(latency_ns);
    }

    uint64_t end_tsc = rdtscp();
    uint64_t total_ns = Timestamp::tsc_to_ns(end_tsc - start_tsc);

    double ns_per_record = static_cast<double>(total_ns) / SAMPLES;

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Samples: " << SAMPLES << std::endl;
    std::cout << "  Nanoseconds per record: " << std::fixed << std::setprecision(2)
              << ns_per_record << " ns" << std::endl;

    std::cout << "\nHistogram Statistics:" << std::endl;
    std::cout << "  Count: " << histogram.count() << std::endl;
    std::cout << "  Min:   " << (histogram.min() / 1000) << " us" << std::endl;
    std::cout << "  p50:   " << (histogram.p50() / 1000) << " us" << std::endl;
    std::cout << "  p99:   " << (histogram.p99() / 1000) << " us" << std::endl;
    std::cout << "  Max:   " << (histogram.max() / 1000) << " us" << std::endl;
    std::cout << "  Mean:  " << (histogram.mean() / 1000) << " us" << std::endl;
}

int main() {
    std::cout << "=== Latency Tracking Benchmarks ===" << std::endl;
    benchmark_latency_histogram();
    return 0;
}
