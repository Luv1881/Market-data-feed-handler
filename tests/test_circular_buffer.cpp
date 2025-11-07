#include "doctest.h"
#include "market_data/lockfree/circular_buffer.hpp"

using namespace market_data;

TEST_CASE("CircularBuffer basic operations") {
    CircularBuffer<int, 16> buffer;

    SUBCASE("Initial state") {
        CHECK(buffer.empty());
        CHECK_FALSE(buffer.full());
        CHECK(buffer.size() == 0);
        CHECK(buffer.capacity() == 16);
    }

    SUBCASE("Push and pop") {
        CHECK(buffer.try_push(42));
        CHECK_FALSE(buffer.empty());
        CHECK(buffer.size() == 1);

        int value;
        CHECK(buffer.try_pop(value));
        CHECK(value == 42);
        CHECK(buffer.empty());
    }

    SUBCASE("Fill buffer") {
        for (int i = 0; i < 15; ++i) {
            CHECK(buffer.try_push(i));
        }
        CHECK(buffer.size() == 15);
        CHECK(buffer.full());

        // Should fail when full
        CHECK_FALSE(buffer.try_push(999));
    }

    SUBCASE("Wraparound") {
        // Fill and drain multiple times
        for (int cycle = 0; cycle < 3; ++cycle) {
            for (int i = 0; i < 10; ++i) {
                CHECK(buffer.try_push(i));
            }

            for (int i = 0; i < 10; ++i) {
                int value;
                CHECK(buffer.try_pop(value));
                CHECK(value == i);
            }
        }
        CHECK(buffer.empty());
    }

    SUBCASE("Peek") {
        buffer.try_push(123);

        int value;
        CHECK(buffer.try_peek(value));
        CHECK(value == 123);

        // Buffer should still have the value
        CHECK_FALSE(buffer.empty());
        CHECK(buffer.try_pop(value));
        CHECK(value == 123);
    }
}

TEST_CASE("CircularBuffer with MarketEvent") {
    CircularBuffer<MarketEvent, 1024> buffer;

    MarketEvent event;
    event.venue_id = 1;
    event.sequence_number = 100;
    event.price = 15000;
    event.event_type = EventType::TRADE;

    CHECK(buffer.try_push(event));

    MarketEvent received;
    CHECK(buffer.try_pop(received));
    CHECK(received.venue_id == 1);
    CHECK(received.sequence_number == 100);
    CHECK(received.price == 15000);
    CHECK(received.event_type == EventType::TRADE);
}
