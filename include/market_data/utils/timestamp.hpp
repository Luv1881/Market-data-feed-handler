#pragma once

#include "../core/common.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>

namespace market_data {

/**
 * High-precision timestamp utilities using TSC (Time Stamp Counter)
 */
class Timestamp {
public:
    /**
     * Initialize TSC calibration
     * Should be called once at program startup
     */
    static void initialize() {
        calibrate_tsc();
    }

    /**
     * Get current TSC value
     */
    static uint64_t now_tsc() {
        return rdtscp();
    }

    /**
     * Get current time in nanoseconds since epoch
     */
    static uint64_t now_ns() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }

    /**
     * Get current time in microseconds since epoch
     */
    static uint64_t now_us() {
        return now_ns() / 1000;
    }

    /**
     * Convert TSC cycles to nanoseconds
     */
    static uint64_t tsc_to_ns(uint64_t tsc) {
        return (tsc * 1000000000ULL) / tsc_frequency_;
    }

    /**
     * Convert TSC cycles to microseconds
     */
    static uint64_t tsc_to_us(uint64_t tsc) {
        return (tsc * 1000000ULL) / tsc_frequency_;
    }

    /**
     * Convert nanoseconds to TSC cycles
     */
    static uint64_t ns_to_tsc(uint64_t ns) {
        return (ns * tsc_frequency_) / 1000000000ULL;
    }

    /**
     * Get TSC frequency in Hz
     */
    static uint64_t get_tsc_frequency() {
        return tsc_frequency_;
    }

    /**
     * Format timestamp as human-readable string
     */
    static std::string format_timestamp(uint64_t timestamp_ns) {
        auto seconds = timestamp_ns / 1000000000ULL;
        auto nanos = timestamp_ns % 1000000000ULL;

        std::time_t time = static_cast<std::time_t>(seconds);
        std::tm tm;
        localtime_r(&time, &tm);

        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

        char result[80];
        std::snprintf(result, sizeof(result), "%s.%09llu", buffer, nanos);
        return std::string(result);
    }

private:
    /**
     * Calibrate TSC frequency
     */
    static void calibrate_tsc() {
        // Measure TSC frequency by comparing with high_resolution_clock
        auto start_time = std::chrono::high_resolution_clock::now();
        uint64_t start_tsc = rdtscp();

        // Sleep for 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto end_time = std::chrono::high_resolution_clock::now();
        uint64_t end_tsc = rdtscp();

        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time).count();

        uint64_t tsc_diff = end_tsc - start_tsc;

        // Calculate frequency
        tsc_frequency_ = (tsc_diff * 1000000000ULL) / duration_ns;
    }

    static inline uint64_t tsc_frequency_ = 0;
};

/**
 * Scoped latency measurement using TSC
 */
class ScopedLatency {
public:
    explicit ScopedLatency(const char* label)
        : label_(label)
        , start_tsc_(rdtscp())
    {}

    ~ScopedLatency() {
        uint64_t end_tsc = rdtscp();
        uint64_t latency_ns = Timestamp::tsc_to_ns(end_tsc - start_tsc_);
        // In production, this would go to a metrics system
        // For now, just measure
        (void)latency_ns;
    }

    /**
     * Get elapsed time in nanoseconds
     */
    uint64_t elapsed_ns() const {
        uint64_t current_tsc = rdtscp();
        return Timestamp::tsc_to_ns(current_tsc - start_tsc_);
    }

    /**
     * Get elapsed time in microseconds
     */
    uint64_t elapsed_us() const {
        return elapsed_ns() / 1000;
    }

private:
    const char* label_;
    uint64_t start_tsc_;
};

} // namespace market_data
