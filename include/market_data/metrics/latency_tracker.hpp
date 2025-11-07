#pragma once

#include "../core/common.hpp"
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>

namespace market_data {

/**
 * Lock-free latency histogram
 *
 * Features:
 * - Lock-free atomic counters
 * - Logarithmic buckets for wide range
 * - Low overhead in hot path
 * - Statistical summary (p50, p99, p99.99)
 */
class LatencyHistogram {
public:
    // Bucket configuration: 0-1us, 1-2us, 2-4us, 4-8us, ..., up to ~1s
    static constexpr std::size_t NUM_BUCKETS = 32;

    LatencyHistogram() {
        reset();
    }

    /**
     * Record a latency sample (in nanoseconds)
     */
    void record(uint64_t latency_ns) {
        std::size_t bucket = get_bucket(latency_ns);
        buckets_[bucket].fetch_add(1, std::memory_order_relaxed);
        total_count_.fetch_add(1, std::memory_order_relaxed);

        // Update min/max
        update_min(latency_ns);
        update_max(latency_ns);

        // Update sum for mean calculation
        sum_.fetch_add(latency_ns, std::memory_order_relaxed);
    }

    /**
     * Get percentile value (in nanoseconds)
     * @param percentile Percentile to compute (0.0 to 1.0)
     */
    uint64_t get_percentile(double percentile) const {
        uint64_t total = total_count_.load(std::memory_order_relaxed);
        if (total == 0) return 0;

        uint64_t target = static_cast<uint64_t>(total * percentile);
        uint64_t cumulative = 0;

        for (std::size_t i = 0; i < NUM_BUCKETS; ++i) {
            cumulative += buckets_[i].load(std::memory_order_relaxed);
            if (cumulative >= target) {
                return get_bucket_upper_bound(i);
            }
        }

        return get_bucket_upper_bound(NUM_BUCKETS - 1);
    }

    /**
     * Get p50 (median) latency
     */
    uint64_t p50() const {
        return get_percentile(0.50);
    }

    /**
     * Get p99 latency
     */
    uint64_t p99() const {
        return get_percentile(0.99);
    }

    /**
     * Get p99.9 latency
     */
    uint64_t p999() const {
        return get_percentile(0.999);
    }

    /**
     * Get p99.99 latency
     */
    uint64_t p9999() const {
        return get_percentile(0.9999);
    }

    /**
     * Get minimum latency
     */
    uint64_t min() const {
        return min_.load(std::memory_order_relaxed);
    }

    /**
     * Get maximum latency
     */
    uint64_t max() const {
        return max_.load(std::memory_order_relaxed);
    }

    /**
     * Get mean latency
     */
    uint64_t mean() const {
        uint64_t total = total_count_.load(std::memory_order_relaxed);
        if (total == 0) return 0;
        return sum_.load(std::memory_order_relaxed) / total;
    }

    /**
     * Get standard deviation (approximate)
     */
    uint64_t stddev() const {
        // Simplified: use range/4 as approximation
        uint64_t range = max() - min();
        return range / 4;
    }

    /**
     * Get total count
     */
    uint64_t count() const {
        return total_count_.load(std::memory_order_relaxed);
    }

    /**
     * Reset histogram
     */
    void reset() {
        for (auto& bucket : buckets_) {
            bucket.store(0, std::memory_order_relaxed);
        }
        total_count_.store(0, std::memory_order_relaxed);
        min_.store(UINT64_MAX, std::memory_order_relaxed);
        max_.store(0, std::memory_order_relaxed);
        sum_.store(0, std::memory_order_relaxed);
    }

    /**
     * Print histogram summary
     */
    void print_summary(const char* label) const {
        // In production, this would go to logging system
        // For now, just compute the stats
        (void)label;
    }

private:
    /**
     * Get bucket index for latency value
     */
    std::size_t get_bucket(uint64_t latency_ns) const {
        if (latency_ns == 0) return 0;

        // Logarithmic buckets: bucket = log2(latency_ns / 1000)
        // Each bucket represents a power-of-2 range in microseconds
        uint64_t latency_us = latency_ns / 1000;
        if (latency_us == 0) return 0;

        std::size_t bucket = 0;
        uint64_t val = latency_us;
        while (val > 0 && bucket < NUM_BUCKETS - 1) {
            val >>= 1;
            ++bucket;
        }

        return std::min(bucket, NUM_BUCKETS - 1);
    }

    /**
     * Get upper bound of bucket (in nanoseconds)
     */
    uint64_t get_bucket_upper_bound(std::size_t bucket) const {
        if (bucket == 0) return 1000; // 1us
        return (1ULL << bucket) * 1000; // 2^bucket microseconds
    }

    /**
     * Update minimum value
     */
    void update_min(uint64_t value) {
        uint64_t current = min_.load(std::memory_order_relaxed);
        while (value < current) {
            if (min_.compare_exchange_weak(current, value,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed)) {
                break;
            }
        }
    }

    /**
     * Update maximum value
     */
    void update_max(uint64_t value) {
        uint64_t current = max_.load(std::memory_order_relaxed);
        while (value > current) {
            if (max_.compare_exchange_weak(current, value,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed)) {
                break;
            }
        }
    }

    CACHE_ALIGNED std::array<std::atomic<uint64_t>, NUM_BUCKETS> buckets_;
    CACHE_ALIGNED std::atomic<uint64_t> total_count_;
    CACHE_ALIGNED std::atomic<uint64_t> min_;
    CACHE_ALIGNED std::atomic<uint64_t> max_;
    CACHE_ALIGNED std::atomic<uint64_t> sum_;
};

/**
 * Metrics collector for market data handler
 */
struct MetricsCollector {
    CACHE_ALIGNED std::atomic<uint64_t> messages_received{0};
    CACHE_ALIGNED std::atomic<uint64_t> messages_processed{0};
    CACHE_ALIGNED std::atomic<uint64_t> messages_dropped{0};
    CACHE_ALIGNED std::atomic<uint64_t> parse_errors{0};
    CACHE_ALIGNED std::atomic<uint64_t> sequence_gaps{0};
    CACHE_ALIGNED std::atomic<uint64_t> queue_full_events{0};

    LatencyHistogram end_to_end_latency;
    LatencyHistogram parse_latency;
    LatencyHistogram queue_latency;

    void record_message_received() {
        messages_received.fetch_add(1, std::memory_order_relaxed);
    }

    void record_message_processed() {
        messages_processed.fetch_add(1, std::memory_order_relaxed);
    }

    void record_message_dropped() {
        messages_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    void record_parse_error() {
        parse_errors.fetch_add(1, std::memory_order_relaxed);
    }

    void record_sequence_gap() {
        sequence_gaps.fetch_add(1, std::memory_order_relaxed);
    }

    void record_queue_full() {
        queue_full_events.fetch_add(1, std::memory_order_relaxed);
    }

    void reset() {
        messages_received.store(0, std::memory_order_relaxed);
        messages_processed.store(0, std::memory_order_relaxed);
        messages_dropped.store(0, std::memory_order_relaxed);
        parse_errors.store(0, std::memory_order_relaxed);
        sequence_gaps.store(0, std::memory_order_relaxed);
        queue_full_events.store(0, std::memory_order_relaxed);

        end_to_end_latency.reset();
        parse_latency.reset();
        queue_latency.reset();
    }
};

} // namespace market_data
