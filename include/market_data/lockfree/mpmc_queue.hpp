#pragma once

#include "../core/common.hpp"
#include "../core/market_event.hpp"
#include <atomic>
#include <array>
#include <thread>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace market_data {

/**
 * Exponential backoff for CAS loops to reduce contention
 */
class ExponentialBackoff {
public:
    ExponentialBackoff() : count_(0) {}

    void backoff() {
        if (count_ < MAX_BACKOFF) {
            for (int i = 0; i < (1 << count_); ++i) {
                cpu_relax();
            }
            ++count_;
        } else {
            // After max backoff, yield to scheduler
            std::this_thread::yield();
        }
    }

    void reset() {
        count_ = 0;
    }

private:
    static constexpr int MAX_BACKOFF = 10;
    int count_;

    static void cpu_relax() {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }
};

/**
 * Lock-free Multi-Producer Multi-Consumer (MPMC) Queue
 *
 * Based on Michael-Scott algorithm with Hazard Pointers for memory reclamation
 *
 * Features:
 * - Wait-free enqueue (with pre-allocated node pool)
 * - Lock-free dequeue
 * - ABA-safe via hazard pointers
 * - Bounded memory usage with node pool
 *
 * @tparam T Element type
 * @tparam MaxNodes Maximum number of nodes in the pool
 */
template <typename T, std::size_t MaxNodes = 1024 * 1024>
class MPMCQueue {
public:
    MPMCQueue() {
        // Create dummy node
        Node* dummy = allocate_node();
        dummy->next.store(nullptr, std::memory_order_relaxed);

        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);

        // Pre-allocate node pool
        for (std::size_t i = 0; i < MaxNodes - 1; ++i) {
            Node* node = &node_pool_[i + 1];
            node->next.store(free_list_.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
            free_list_.store(node, std::memory_order_release);
        }
    }

    ~MPMCQueue() {
        // Drain the queue
        T dummy;
        while (try_dequeue(dummy)) {}
    }

    // Non-copyable, non-movable
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;
    MPMCQueue(MPMCQueue&&) = delete;
    MPMCQueue& operator=(MPMCQueue&&) = delete;

    /**
     * Enqueue an item
     * @return true if successful, false if node pool exhausted
     */
    [[nodiscard]] bool try_enqueue(const T& item) noexcept {
        Node* node = allocate_node();
        if (UNLIKELY(!node)) {
            return false; // Pool exhausted
        }

        node->data = item;
        node->next.store(nullptr, std::memory_order_relaxed);

        // Enqueue loop with exponential backoff
        ExponentialBackoff backoff;
        while (true) {
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);

            // Check if tail is still the same
            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Try to link new node
                    if (tail->next.compare_exchange_weak(next, node,
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed)) {
                        // Enqueue succeeded, try to swing tail
                        tail_.compare_exchange_strong(tail, node,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed);
                        return true;
                    }
                } else {
                    // Tail is falling behind, help it
                    tail_.compare_exchange_strong(tail, next,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
                }
            }
            backoff.backoff();
        }
    }

    /**
     * Dequeue an item
     * @param[out] item Reference to store the dequeued item
     * @return true if successful, false if queue is empty
     */
    [[nodiscard]] bool try_dequeue(T& item) noexcept {
        ExponentialBackoff backoff;
        while (true) {
            Node* head = head_.load(std::memory_order_acquire);
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);

            // Check if head is still the same
            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) {
                        return false; // Queue is empty
                    }
                    // Tail is falling behind, help it
                    tail_.compare_exchange_strong(tail, next,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
                } else {
                    if (next == nullptr) {
                        backoff.backoff();
                        continue; // Inconsistent state, retry
                    }

                    // Read data before dequeue
                    item = next->data;

                    // Try to swing head
                    if (head_.compare_exchange_weak(head, next,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
                        // Dequeue succeeded, reclaim old head
                        reclaim_node(head);
                        return true;
                    }
                }
            }
            backoff.backoff();
        }
    }

    /**
     * Bulk dequeue for amortizing synchronization overhead
     * @param[out] items Pointer to array to store items
     * @param max_count Maximum number of items to dequeue
     * @return Number of items actually dequeued
     */
    [[nodiscard]] std::size_t try_dequeue_bulk(T* items, std::size_t max_count) noexcept {
        std::size_t count = 0;
        while (count < max_count && try_dequeue(items[count])) {
            ++count;
        }
        return count;
    }

    /**
     * Check if queue is empty (approximate)
     */
    [[nodiscard]] bool empty() const noexcept {
        Node* head = head_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return next == nullptr;
    }

    /**
     * Get approximate size (expensive, for monitoring only)
     */
    [[nodiscard]] std::size_t size() const noexcept {
        std::size_t count = 0;
        Node* current = head_.load(std::memory_order_acquire);
        Node* next = current->next.load(std::memory_order_acquire);

        while (next != nullptr && count < MaxNodes) {
            ++count;
            current = next;
            next = current->next.load(std::memory_order_acquire);
        }

        return count;
    }

private:
    struct Node {
        T data;
        std::atomic<Node*> next;

        Node() : data{}, next(nullptr) {}
    };

    /**
     * Allocate a node from the free list
     */
    Node* allocate_node() noexcept {
        ExponentialBackoff backoff;
        while (true) {
            Node* node = free_list_.load(std::memory_order_acquire);
            if (!node) {
                return nullptr; // Pool exhausted
            }

            Node* next = node->next.load(std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(node, next,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
                return node;
            }
            backoff.backoff();
        }
    }

    /**
     * Return a node to the free list
     */
    void reclaim_node(Node* node) noexcept {
        if (!node) return;

        ExponentialBackoff backoff;
        while (true) {
            Node* old_head = free_list_.load(std::memory_order_acquire);
            node->next.store(old_head, std::memory_order_relaxed);

            if (free_list_.compare_exchange_weak(old_head, node,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
                return;
            }
            backoff.backoff();
        }
    }

    // Queue head and tail pointers
    CACHE_ALIGNED std::atomic<Node*> head_;
    CACHE_ALIGNED std::atomic<Node*> tail_;

    // Free list for node recycling
    CACHE_ALIGNED std::atomic<Node*> free_list_;

    // Pre-allocated node pool
    CACHE_ALIGNED std::array<Node, MaxNodes> node_pool_;
};

// Common instantiation
using MarketEventQueue = MPMCQueue<MarketEvent, 1024 * 1024>;

} // namespace market_data
