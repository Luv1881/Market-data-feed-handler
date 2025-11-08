#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Building Market Data Handler ===${NC}"

# Default values
BUILD_TYPE="Release"
CLEAN=false
RUN_TESTS=false
RUN_BENCHMARKS=false
ENABLE_SANITIZERS=false
COMPILER="g++"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --test)
            RUN_TESTS=true
            shift
            ;;
        --bench)
            RUN_BENCHMARKS=true
            shift
            ;;
        --sanitize)
            ENABLE_SANITIZERS=true
            shift
            ;;
        --clang)
            COMPILER="clang++"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug        Build in Debug mode (default: Release)"
            echo "  --clean        Clean build directory before building"
            echo "  --test         Run tests after building"
            echo "  --bench        Run benchmarks after building"
            echo "  --sanitize     Enable AddressSanitizer and UBSan"
            echo "  --clang        Use clang++ compiler (default: g++)"
            echo "  --help         Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure CMake
echo -e "${GREEN}Configuring CMake...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_CXX_COMPILER=${COMPILER}"

if [ "$ENABLE_SANITIZERS" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DENABLE_ASAN=ON -DENABLE_UBSAN=ON"
fi

cmake .. $CMAKE_ARGS

# Build
echo -e "${GREEN}Building...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    echo -e "${GREEN}Running tests...${NC}"
    ctest --output-on-failure
fi

# Run benchmarks if requested
if [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "${GREEN}Running benchmarks...${NC}"
    echo -e "${YELLOW}Circular Buffer Benchmark:${NC}"
    ./benchmarks/benchmark_circular_buffer
    echo -e "${YELLOW}Latency Benchmark:${NC}"
    ./benchmarks/benchmark_latency
fi

echo -e "${GREEN}=== Done ===${NC}"
echo ""
echo "Executables:"
echo "  ./market_data_handler [duration_seconds]"
echo "  ./examples/simple_example"
echo "  ./tests/market_data_tests"
echo "  ./benchmarks/benchmark_circular_buffer"
echo "  ./benchmarks/benchmark_latency"
