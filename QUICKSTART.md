# Quick Start Guide

Get up and running with the Market Data Handler in 5 minutes.

## Prerequisites

Install required packages on Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build clang
```

## Build and Run

### Option 1: Using the build script (Recommended)

```bash
# Build the project
./build.sh

# Build and run tests
./build.sh --test

# Build with sanitizers (for development)
./build.sh --debug --sanitize

# Build and run benchmarks
./build.sh --bench
```

### Option 2: Manual build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Run the Application

```bash
# From the build directory
./market_data_handler 10

# This will run the market data handler for 10 seconds
# You'll see real-time statistics including:
#   - Messages received/processed
#   - Latency percentiles (p50, p99, p99.9, p99.99)
#   - Queue statistics
```

### Expected Output

```
=== Ultra-Low-Latency Market Data Feed Handler ===
C++20 High-Performance Trading System

TSC frequency: 2400000000 Hz
Number of CPUs: 8
Cache line size: 64 bytes
MarketEvent size: 64 bytes

=== Starting Market Data Feed Simulation ===

=== Statistics ===
Received:  1000000
Processed: 1000000
Dropped:   0

Latency (microseconds):
  Min:         1 us
  p50:         2 us
  p99:         5 us
  p99.9:      10 us
  p99.99:     15 us
  Max:        25 us
  Mean:        3 us
```

## Run Examples

```bash
# Simple example
./examples/simple_example

# Run unit tests
./tests/market_data_tests

# Run benchmarks
./benchmarks/market_data_benchmarks
```

## Understanding the Output

### Latency Metrics

- **p50**: Median latency - half of all messages are processed faster than this
- **p99**: 99th percentile - 99% of messages are faster than this
- **p99.9**: 99.9th percentile - ultra-low latency target
- **p99.99**: 99.99th percentile - worst-case latency (excluding outliers)

### Target Performance

According to the specifications:
- **p50**: < 3 microseconds
- **p99**: < 8 microseconds
- **p99.99**: < 15 microseconds

## Optimize for Your System

### 1. Check CPU Information

```bash
# Check if TSC is constant
grep constant_tsc /proc/cpuinfo

# Check CPU frequency
lscpu | grep MHz
```

### 2. Optional: Enable CPU Isolation

For best performance on a multi-core system:

```bash
# Edit GRUB configuration
sudo nano /etc/default/grub

# Add to GRUB_CMDLINE_LINUX_DEFAULT:
# isolcpus=2-7 nohz_full=2-7 rcu_nocbs=2-7

# Update GRUB
sudo update-grub

# Reboot
sudo reboot
```

### 3. Optional: Enable Huge Pages

```bash
# Temporary (until reboot)
sudo sysctl -w vm.nr_hugepages=1024

# Permanent
echo "vm.nr_hugepages=1024" | sudo tee -a /etc/sysctl.conf
```

### 4. Run with Real-Time Priority

```bash
# Add to /etc/security/limits.conf:
your_user    soft    rtprio    99
your_user    hard    rtprio    99

# Log out and back in for changes to take effect
```

## Common Issues

### Issue: "Permission denied" when setting real-time priority

**Solution**: Configure limits.conf as shown above.

### Issue: Build fails with C++20 errors

**Solution**: Use a newer compiler:
```bash
./build.sh --clang  # Use Clang instead of GCC
```

### Issue: Low performance / high latency

**Solution**:
1. Build in Release mode: `./build.sh` (default)
2. Check CPU frequency scaling: `cpupower frequency-info`
3. Set performance governor: `sudo cpupower frequency-set -g performance`

### Issue: Tests fail

**Solution**: Run with verbose output:
```bash
cd build
ctest --verbose --output-on-failure
```

## Next Steps

1. Read [README.md](README.md) for detailed documentation
2. Review [Instructions.md](Instructions.md) for full specifications
3. Explore the code in [include/market_data/](include/market_data/)
4. Modify [src/main.cpp](src/main.cpp) to add your own logic
5. Write custom protocol parsers in [include/market_data/protocol/](include/market_data/protocol/)

## Development Workflow

```bash
# Make code changes
vim src/main.cpp

# Rebuild (incremental)
cd build && make -j$(nproc)

# Run tests
ctest

# Run with sanitizers (debug build)
./build.sh --clean --debug --sanitize --test

# Profile performance
perf stat -e cycles,cache-misses ./market_data_handler 10
```

## Getting Help

- Check the [README.md](README.md) for detailed documentation
- Review the [Instructions.md](Instructions.md) for specifications
- Look at example code in [examples/](examples/)
- Read inline code comments in header files

Happy coding! 🚀
