#include "doctest.h"
#include "market_data/lockfree/mpmc_queue.hpp"

using namespace market_data;

TEST_CASE("MPMCQueue basic operations") {
    MPMCQueue<int, 1024> queue;

    SUBCASE("Initial state") {
        CHECK(queue.empty());
    }

    SUBCASE("Enqueue and dequeue") {
        CHECK(queue.try_enqueue(42));
        CHECK_FALSE(queue.empty());

        int value;
        CHECK(queue.try_dequeue(value));
        CHECK(value == 42);
        CHECK(queue.empty());
    }

    SUBCASE("Multiple items") {
        for (int i = 0; i < 100; ++i) {
            CHECK(queue.try_enqueue(i));
        }

        for (int i = 0; i < 100; ++i) {
            int value;
            CHECK(queue.try_dequeue(value));
            CHECK(value == i);
        }

        CHECK(queue.empty());
    }

    SUBCASE("Bulk dequeue") {
        for (int i = 0; i < 64; ++i) {
            queue.try_enqueue(i);
        }

        int items[64];
        std::size_t count = queue.try_dequeue_bulk(items, 64);
        CHECK(count == 64);

        for (int i = 0; i < 64; ++i) {
            CHECK(items[i] == i);
        }
    }
}

TEST_CASE("MPMCQueue with MarketEvent") {
    MPMCQueue<MarketEvent, 1024> queue;

    MarketEvent event;
    event.venue_id = 2;
    event.sequence_number = 200;
    event.event_type = EventType::QUOTE;

    CHECK(queue.try_enqueue(event));

    MarketEvent received;
    CHECK(queue.try_dequeue(received));
    CHECK(received.venue_id == 2);
    CHECK(received.sequence_number == 200);
    CHECK(received.event_type == EventType::QUOTE);
}
