#include "doctest.h"
#include "market_data/memory/memory_pool.hpp"
#include <vector>

using namespace market_data;

TEST_CASE("MemoryPool basic operations") {
    MemoryPool<int, 100> pool;

    SUBCASE("Initial state") {
        CHECK(pool.capacity() == 100);
        CHECK(pool.available() > 0);
    }

    SUBCASE("Allocate and deallocate") {
        int* ptr = pool.allocate();
        CHECK(ptr != nullptr);

        *ptr = 42;
        CHECK(*ptr == 42);

        pool.deallocate(ptr);
    }

    SUBCASE("Multiple allocations") {
        std::vector<int*> ptrs;

        for (int i = 0; i < 50; ++i) {
            int* ptr = pool.allocate();
            CHECK(ptr != nullptr);
            *ptr = i;
            ptrs.push_back(ptr);
        }

        for (int i = 0; i < 50; ++i) {
            CHECK(*ptrs[i] == i);
            pool.deallocate(ptrs[i]);
        }
    }

    SUBCASE("Exhaust pool") {
        std::vector<int*> ptrs;

        // Allocate all slots
        for (std::size_t i = 0; i < pool.capacity(); ++i) {
            int* ptr = pool.allocate();
            if (ptr) {
                ptrs.push_back(ptr);
            }
        }

        // Pool should be exhausted
        CHECK(pool.allocate() == nullptr);

        // Deallocate all
        for (auto ptr : ptrs) {
            pool.deallocate(ptr);
        }
    }
}

TEST_CASE("MemoryPool with MarketEvent") {
    MemoryPool<MarketEvent, 1000> pool;

    MarketEvent* event = pool.allocate();
    CHECK(event != nullptr);

    event->venue_id = 3;
    event->sequence_number = 300;
    event->event_type = EventType::BOOK_UPDATE;

    CHECK(event->venue_id == 3);
    CHECK(event->sequence_number == 300);

    pool.deallocate(event);
}
