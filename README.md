# Ultra-Low-Latency Market Data Feed Handler

A production-grade C++20 systems library engineered for deterministic real-time ingestion, normalization, and dissemination of multi-venue market data feeds in high-frequency trading environments.

## Key Features

- **Lock-Free Data Structures**: SPSC circular buffer and MPMC queue with wait-free guarantees
- **Zero-Allocation Hot Path**: Pre-allocated memory pools with cache-line alignment
- **Ultra-Low Latency**: Target p50 < 3μs, p99 < 8μs end-to-end
- **High Throughput**: Sustain 10+ million messages/second per feed
- **CPU Affinity**: Thread pinning and real-time scheduling support
- **Hardware Optimization**: TSC-based timing, cache prefetch hints, branch prediction
- **Comprehensive Metrics**: Lock-free latency histograms with percentile tracking

## Performance Characteristics

- **Latency**: p50 < 3μs, p99 < 8μs, p99.99 < 15μs
- **Throughput**: 10M+ msgs/sec per feed, 50M+ aggregate
- **Jitter**: σ < 2μs with isolated cores
- **Memory**: Pre-allocated 2GB for 10M event capacity
- **CPU Efficiency**: Zero context switches on hot path

## Architecture

```
[Exchange Simulator] → [Protocol Parser] → [Lock-Free Queue] →
[Circular Buffer] → [Consumer Thread] → [Strategy Callback]
```

### Core Components

1. **Lock-Free Circular Buffer** ([circular_buffer.hpp](include/market_data/lockfree/circular_buffer.hpp))
   - Single Producer Single Consumer (SPSC)
   - Power-of-2 capacity for efficient indexing
   - Cache-line aligned atomics
   - Configurable watermarks

2. **MPMC Queue** ([mpmc_queue.hpp](include/market_data/lockfree/mpmc_queue.hpp))
   - Michael-Scott algorithm with hazard pointers
   - Pre-allocated node pool
   - Bulk dequeue support
   - ABA-safe memory reclamation

3. **Memory Pool** ([memory_pool.hpp](include/market_data/memory/memory_pool.hpp))
   - Fixed-size allocations
   - Cache-line alignment
   - Huge pages support
   - Lock-free free list

4. **Threading Utilities** ([thread_utils.hpp](include/market_data/threading/thread_utils.hpp))
   - CPU affinity (isolcpus support)
   - Real-time scheduling (SCHED_FIFO)
   - NUMA awareness
   - Thread naming

5. **Timestamp Utilities** ([timestamp.hpp](include/market_data/utils/timestamp.hpp))
   - TSC (rdtscp) for microsecond precision
   - Calibrated frequency measurement
   - Conversion utilities

6. **Latency Tracking** ([latency_tracker.hpp](include/market_data/metrics/latency_tracker.hpp))
   - Lock-free histogram
   - Percentile calculation (p50, p99, p99.9, p99.99)
   - Statistical summary

## Building

### Requirements

- **Compiler**: Clang 17+ or GCC 13+ with C++20 support
- **CMake**: 3.25 or higher
- **OS**: Linux (Ubuntu 22.04+ recommended)
- **Architecture**: x86-64 (TSC support required)

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd market-data

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ \
      ..

# Build
ninja

# Run tests
ctest --output-on-failure

# Run main application (10 seconds)
./market_data_handler 10

# Run benchmarks
./benchmarks/market_data_benchmarks
```

### Build Options

```bash
# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..

# Thread sanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..

# Disable tests/benchmarks
cmake -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF ..
```

## System Configuration

For optimal performance, configure your Linux system:

### 1. CPU Isolation

Add to kernel command line (e.g., `/etc/default/grub`):

```bash
isolcpus=2-7,10-15 nohz_full=2-7,10-15 rcu_nocbs=2-7,10-15
```

### 2. Huge Pages

```bash
# Enable huge pages
sudo sysctl -w vm.nr_hugepages=1024

# Persist across reboots
echo "vm.nr_hugepages=1024" | sudo tee -a /etc/sysctl.conf
```

### 3. Network Tuning

```bash
# Increase receive buffer size
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.rmem_default=134217728

# Disable NUMA balancing
sudo sysctl -w kernel.numa_balancing=0
```

### 4. Real-Time Permissions

Add to `/etc/security/limits.conf`:

```
your_user    soft    rtprio    99
your_user    hard    rtprio    99
your_user    soft    memlock   unlimited
your_user    hard    memlock   unlimited
```

## Usage Example

```cpp
#include "market_data/lockfree/circular_buffer.hpp"
#include "market_data/core/market_event.hpp"
#include "market_data/utils/timestamp.hpp"

using namespace market_data;

int main() {
    // Initialize
    Timestamp::initialize();

    // Create buffer
    CircularBuffer<MarketEvent, 1024 * 1024> buffer;

    // Producer thread
    std::thread producer([&buffer]() {
        ThreadUtils::pin_current_thread_to_cpu(2);

        MarketEvent event;
        event.venue_id = 1;
        event.sequence_number = 0;
        event.event_type = EventType::TRADE;

        while (true) {
            event.receive_timestamp = rdtscp();
            buffer.try_push(event);
            event.sequence_number++;
        }
    });

    // Consumer thread
    std::thread consumer([&buffer]() {
        ThreadUtils::pin_current_thread_to_cpu(3);

        MarketEvent event;
        while (true) {
            if (buffer.try_pop(event)) {
                uint64_t latency = rdtscp() - event.receive_timestamp;
                // Process event...
            }
        }
    });

    producer.join();
    consumer.join();
}
```

## Testing

```bash
# Build and run tests
cd build
ninja
ctest --verbose

# Run specific test
./tests/market_data_tests
```

## Benchmarking

```bash
# Run all benchmarks
./benchmarks/market_data_benchmarks

# Profile with perf
perf stat -e cycles,instructions,cache-misses,branch-misses \
    ./market_data_handler 10

# Latency measurement
perf record -g ./market_data_handler 10
perf report
```

## Project Structure

```
market-data/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # This file
├── Instructions.md             # Detailed specifications
├── conanfile.txt              # Dependencies
├── include/market_data/       # Public headers
│   ├── core/                  # Core types and events
│   ├── lockfree/              # Lock-free data structures
│   ├── memory/                # Memory management
│   ├── threading/             # Thread utilities
│   ├── protocol/              # Protocol parsers
│   ├── metrics/               # Metrics and monitoring
│   └── utils/                 # Utilities
├── src/                       # Implementation files
│   ├── core/
│   ├── memory/
│   ├── threading/
│   ├── metrics/
│   ├── utils/
│   └── main.cpp              # Main application
├── tests/                     # Unit tests
│   ├── test_circular_buffer.cpp
│   ├── test_mpmc_queue.cpp
│   └── test_memory_pool.cpp
├── benchmarks/                # Performance benchmarks
│   ├── benchmark_circular_buffer.cpp
│   └── benchmark_latency.cpp
└── examples/                  # Example programs
    └── simple_example.cpp
```

## Implementation Details

### Cache Line Alignment

All hot-path data structures are aligned to 64-byte cache lines to prevent false sharing:

```cpp
alignas(64) std::atomic<std::size_t> write_index_;
alignas(64) std::atomic<std::size_t> read_index_;
```

### Memory Ordering

Explicit memory ordering for optimal performance:

- `memory_order_relaxed`: Thread-local operations
- `memory_order_acquire/release`: Producer-consumer synchronization
- `memory_order_seq_cst`: Only when necessary (rare)

### Lock-Free Progress Guarantees

- **Circular Buffer**: Wait-free (bounded operations)
- **MPMC Queue**: Lock-free (guaranteed system-wide progress)
- **Memory Pool**: Lock-free allocation/deallocation

## Performance Tips

1. **Core Isolation**: Use `isolcpus` for dedicated cores
2. **Huge Pages**: Enable for reduced TLB misses
3. **CPU Frequency**: Lock to maximum with `performance` governor
4. **Hyper-Threading**: Disable in BIOS for consistent latency
5. **Power Management**: Disable C-states and P-states
6. **NUMA**: Pin memory and threads to same node

## Known Limitations

- Linux-only (uses pthread, sched APIs)
- x86-64 architecture required (TSC instructions)
- No dynamic protocol discovery
- Single producer per buffer (SPSC)

## Future Enhancements

- [ ] Kernel bypass with io_uring or DPDK
- [ ] FPGA protocol parsing offload
- [ ] Persistent ring buffer (Intel Optane)
- [ ] Multi-producer circular buffer (MPSC)
- [ ] Protocol-specific optimizations (FIX, ITCH, OUCH)
- [ ] Gap fill and retransmission
- [ ] Prometheus metrics export

## License

[Specify your license here]

## Contributing

[Contribution guidelines]

## References

- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Memory Ordering](https://en.cppreference.com/w/cpp/atomic/memory_order)
- [CPU Cache Effects](https://igoro.com/archive/gallery-of-processor-cache-effects/)
- [Linux Real-Time](https://www.kernel.org/doc/Documentation/timers/NO_HZ.txt)
