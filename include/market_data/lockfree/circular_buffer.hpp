#pragma once

#include "../core/common.hpp"
#include <atomic>
#include <array>
#include <type_traits>

namespace market_data {

/**
 * Lock-free Single Producer Single Consumer (SPSC) Circular Buffer
 *
 * Features:
 * - Wait-free for single producer/consumer
 * - Zero allocation after construction
 * - Cache-line aligned atomics to prevent false sharing
 * - Power-of-2 capacity for efficient modulo operations
 * - Configurable watermark monitoring
 *
 * @tparam T Element type (should be trivially copyable)
 * @tparam Capacity Buffer capacity (must be power of 2)
 */
template <typename T, std::size_t Capacity>
class CircularBuffer {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(is_power_of_2(Capacity), "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be greater than 0");

public:
    CircularBuffer()
        : write_index_(0)
        , read_index_(0)
        , high_watermark_(static_cast<std::size_t>(Capacity * 0.9))
        , low_watermark_(static_cast<std::size_t>(Capacity * 0.1))
    {}

    ~CircularBuffer() = default;

    // Non-copyable, non-movable
    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;
    CircularBuffer(CircularBuffer&&) = delete;
    CircularBuffer& operator=(CircularBuffer&&) = delete;

    /**
     * Try to push an element (producer side)
     * @return true if successful, false if buffer is full
     */
    [[nodiscard]] bool try_push(const T& item) noexcept {
        const std::size_t current_write = write_index_.load(std::memory_order_relaxed);
        const std::size_t next_write = increment(current_write);

        // Check if buffer is full
        if (UNLIKELY(next_write == read_index_.load(std::memory_order_acquire))) {
            return false;
        }

        // Write data
        buffer_[current_write] = item;

        // Publish write with release semantics
        write_index_.store(next_write, std::memory_order_release);

        return true;
    }

    /**
     * Try to pop an element (consumer side)
     * @param[out] item Reference to store the popped item
     * @return true if successful, false if buffer is empty
     */
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const std::size_t current_read = read_index_.load(std::memory_order_relaxed);

        // Check if buffer is empty
        if (UNLIKELY(current_read == write_index_.load(std::memory_order_acquire))) {
            return false;
        }

        // Read data
        item = buffer_[current_read];

        // Publish read with release semantics
        read_index_.store(increment(current_read), std::memory_order_release);

        return true;
    }

    /**
     * Peek at the front element without removing it
     * @param[out] item Reference to store the peeked item
     * @return true if successful, false if buffer is empty
     */
    [[nodiscard]] bool try_peek(T& item) const noexcept {
        const std::size_t current_read = read_index_.load(std::memory_order_relaxed);

        // Check if buffer is empty
        if (UNLIKELY(current_read == write_index_.load(std::memory_order_acquire))) {
            return false;
        }

        // Read data without advancing read index
        item = buffer_[current_read];
        return true;
    }

    /**
     * Get current size (approximate, for monitoring only)
     */
    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t write = write_index_.load(std::memory_order_acquire);
        const std::size_t read = read_index_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (Capacity - read + write);
    }

    /**
     * Check if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return read_index_.load(std::memory_order_acquire) ==
               write_index_.load(std::memory_order_acquire);
    }

    /**
     * Check if buffer is full (approximate)
     */
    [[nodiscard]] bool full() const noexcept {
        const std::size_t current_write = write_index_.load(std::memory_order_acquire);
        const std::size_t next_write = increment(current_write);
        return next_write == read_index_.load(std::memory_order_acquire);
    }

    /**
     * Get capacity
     */
    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return Capacity;
    }

    /**
     * Check if high watermark exceeded
     */
    [[nodiscard]] bool high_watermark_exceeded() const noexcept {
        return size() >= high_watermark_;
    }

    /**
     * Check if below low watermark
     */
    [[nodiscard]] bool below_low_watermark() const noexcept {
        return size() <= low_watermark_;
    }

    /**
     * Set watermarks (for monitoring)
     */
    void set_watermarks(std::size_t low, std::size_t high) noexcept {
        low_watermark_ = low;
        high_watermark_ = high;
    }

    /**
     * Reset buffer (NOT thread-safe - call only when no concurrent access)
     */
    void reset() noexcept {
        write_index_.store(0, std::memory_order_relaxed);
        read_index_.store(0, std::memory_order_relaxed);
    }

private:
    /**
     * Increment index with wrapping (using bitmask for power-of-2)
     */
    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) & (Capacity - 1);
    }

    // Cache-line aligned atomic indices to prevent false sharing
    CACHE_ALIGNED std::atomic<std::size_t> write_index_;
    CACHE_ALIGNED std::atomic<std::size_t> read_index_;

    // Buffer storage
    CACHE_ALIGNED std::array<T, Capacity> buffer_;

    // Watermark thresholds
    std::size_t high_watermark_;
    std::size_t low_watermark_;
};

// Common instantiations for market data
using MarketEventBuffer = CircularBuffer<MarketEvent, 1024 * 1024>;  // 1M events
using LargeEventBuffer = CircularBuffer<MarketEvent, 10 * 1024 * 1024>; // 10M events

} // namespace market_data
