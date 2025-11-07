#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

namespace market_data {

// Cache line size for alignment and padding
#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64; // Conservative default for x86-64
#endif

// Align to cache line to prevent false sharing
#define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)

// Likely/unlikely hints for branch prediction
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// Prefetch hints
#define PREFETCH_READ(addr)  __builtin_prefetch((addr), 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)

// Compiler barrier
#define COMPILER_BARRIER() asm volatile("" ::: "memory")

// CPU pause for spin loops
inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    COMPILER_BARRIER();
#endif
}

// Read timestamp counter
inline uint64_t rdtsc() {
#if defined(__x86_64__)
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    return 0; // Fallback for non-x86 architectures
#endif
}

// Read timestamp counter with serialization
inline uint64_t rdtscp() {
#if defined(__x86_64__)
    uint32_t lo, hi;
    uint32_t aux;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    return 0;
#endif
}

// Power of 2 check
constexpr bool is_power_of_2(std::size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

// Next power of 2
constexpr std::size_t next_power_of_2(std::size_t n) {
    if (n == 0) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

// Symbol type (8 bytes, cache-friendly)
struct Symbol {
    char data[8];

    constexpr Symbol() : data{} {}

    explicit Symbol(const char* str) : data{} {
        for (int i = 0; i < 8 && str[i]; ++i) {
            data[i] = str[i];
        }
    }

    bool operator==(const Symbol& other) const {
        return *reinterpret_cast<const uint64_t*>(data) ==
               *reinterpret_cast<const uint64_t*>(other.data);
    }

    bool operator!=(const Symbol& other) const {
        return !(*this == other);
    }
};

// Hash function for Symbol
struct SymbolHash {
    std::size_t operator()(const Symbol& s) const noexcept {
        return *reinterpret_cast<const uint64_t*>(s.data);
    }
};

} // namespace market_data
