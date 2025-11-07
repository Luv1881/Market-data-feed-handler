#pragma once

#include "../core/common.hpp"
#include "../core/market_event.hpp"
#include <atomic>
#include <array>
#include <memory>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>

namespace market_data {

/**
 * Fixed-size memory pool with cache-line alignment
 *
 * Features:
 * - Pre-allocated memory blocks
 * - Lock-free allocation/deallocation
 * - Cache-line aligned allocations
 * - Optional huge pages support
 * - Zero fragmentation
 *
 * @tparam T Object type
 * @tparam NumSlots Number of slots in the pool
 */
template <typename T, std::size_t NumSlots>
class MemoryPool {
public:
    static constexpr std::size_t SLOT_SIZE =
        ((sizeof(T) + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;

    explicit MemoryPool(bool use_huge_pages = false)
        : use_huge_pages_(use_huge_pages)
        , free_list_(nullptr)
    {
        // Allocate memory
        if (use_huge_pages_) {
            allocate_huge_pages();
        } else {
            allocate_normal();
        }

        // Initialize free list
        for (std::size_t i = 0; i < NumSlots; ++i) {
            void* slot = get_slot(i);
            push_free_list(static_cast<FreeNode*>(slot));
        }
    }

    ~MemoryPool() {
        if (use_huge_pages_) {
            munmap(memory_, total_size_);
        } else {
            std::free(memory_);
        }
    }

    // Non-copyable, non-movable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    /**
     * Allocate an object from the pool
     * @return Pointer to allocated object, or nullptr if pool exhausted
     */
    [[nodiscard]] T* allocate() noexcept {
        FreeNode* node = pop_free_list();
        if (UNLIKELY(!node)) {
            return nullptr; // Pool exhausted
        }
        return reinterpret_cast<T*>(node);
    }

    /**
     * Deallocate an object back to the pool
     * @param ptr Pointer to object to deallocate
     */
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;

        // Destroy object (if needed)
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }

        // Return to free list
        push_free_list(reinterpret_cast<FreeNode*>(ptr));
    }

    /**
     * Get number of available slots (approximate)
     */
    [[nodiscard]] std::size_t available() const noexcept {
        std::size_t count = 0;
        FreeNode* node = free_list_.load(std::memory_order_acquire);
        while (node && count < NumSlots) {
            ++count;
            node = node->next.load(std::memory_order_relaxed);
        }
        return count;
    }

    /**
     * Get total number of slots
     */
    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return NumSlots;
    }

    /**
     * Check if pool is using huge pages
     */
    [[nodiscard]] bool using_huge_pages() const noexcept {
        return use_huge_pages_;
    }

private:
    struct FreeNode {
        std::atomic<FreeNode*> next;
    };

    /**
     * Allocate memory with huge pages
     */
    void allocate_huge_pages() {
        total_size_ = NumSlots * SLOT_SIZE;

        // Align to huge page boundary (2MB)
        constexpr std::size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024;
        total_size_ = ((total_size_ + HUGE_PAGE_SIZE - 1) / HUGE_PAGE_SIZE) * HUGE_PAGE_SIZE;

        memory_ = mmap(nullptr, total_size_,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                      -1, 0);

        if (memory_ == MAP_FAILED) {
            // Fallback to normal allocation
            use_huge_pages_ = false;
            allocate_normal();
        }
    }

    /**
     * Allocate memory normally (with alignment)
     */
    void allocate_normal() {
        total_size_ = NumSlots * SLOT_SIZE + CACHE_LINE_SIZE;

        void* raw = std::malloc(total_size_);
        if (!raw) {
            // Cannot throw exceptions (compiled with -fno-exceptions)
            std::abort();
        }

        // Align to cache line
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(raw);
        std::uintptr_t aligned = (addr + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
        memory_ = reinterpret_cast<void*>(aligned);
    }

    /**
     * Get pointer to slot
     */
    void* get_slot(std::size_t index) noexcept {
        char* base = static_cast<char*>(memory_);
        return base + (index * SLOT_SIZE);
    }

    /**
     * Push node to free list (lock-free)
     */
    void push_free_list(FreeNode* node) noexcept {
        FreeNode* old_head = free_list_.load(std::memory_order_relaxed);
        do {
            node->next.store(old_head, std::memory_order_relaxed);
        } while (!free_list_.compare_exchange_weak(old_head, node,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed));
    }

    /**
     * Pop node from free list (lock-free)
     */
    FreeNode* pop_free_list() noexcept {
        FreeNode* node = free_list_.load(std::memory_order_acquire);
        while (node) {
            FreeNode* next = node->next.load(std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(node, next,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
                return node;
            }
        }
        return nullptr;
    }

    bool use_huge_pages_;
    void* memory_;
    std::size_t total_size_;

    CACHE_ALIGNED std::atomic<FreeNode*> free_list_;
};

/**
 * C++20 PMR-compatible allocator using MemoryPool
 */
template <typename T>
class PooledAllocator {
public:
    using value_type = T;

    explicit PooledAllocator(MemoryPool<T, 1024 * 1024>& pool) noexcept
        : pool_(&pool)
    {}

    template <typename U>
    PooledAllocator(const PooledAllocator<U>& other) noexcept
        : pool_(reinterpret_cast<MemoryPool<T, 1024 * 1024>*>(other.pool_))
    {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n != 1) {
            // Only single-object allocation supported, compiled with -fno-exceptions
            std::abort();
        }
        T* ptr = pool_->allocate();
        if (!ptr) {
            // Pool exhausted, compiled with -fno-exceptions
            std::abort();
        }
        return ptr;
    }

    void deallocate(T* ptr, std::size_t) noexcept {
        pool_->deallocate(ptr);
    }

    template <typename U>
    bool operator==(const PooledAllocator<U>& other) const noexcept {
        return pool_ == other.pool_;
    }

    template <typename U>
    bool operator!=(const PooledAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    template <typename> friend class PooledAllocator;
    MemoryPool<T, 1024 * 1024>* pool_;
};

// Common instantiations
using MarketEventPool = MemoryPool<MarketEvent, 10 * 1024 * 1024>; // 10M events

} // namespace market_data
