#include "market_data/lockfree/circular_buffer.hpp"
#include "market_data/core/market_event.hpp"
#include "market_data/utils/timestamp.hpp"
#include <iostream>

using namespace market_data;

int main() {
    std::cout << "=== Simple Market Data Example ===" << std::endl;

    // Initialize timestamp utilities
    Timestamp::initialize();

    // Create a circular buffer
    CircularBuffer<MarketEvent, 1024> buffer;

    std::cout << "Buffer capacity: " << buffer.capacity() << std::endl;

    // Create a market event
    MarketEvent event;
    event.venue_id = 1;
    event.sequence_number = 100;
    event.event_type = EventType::TRADE;
    event.symbol = Symbol("AAPL");
    event.price = 15000000000LL; // $150.00
    event.quantity = 100 * 100000000LL; // 100 shares
    event.side = Side::BID;
    event.exchange_timestamp = Timestamp::now_ns();
    event.receive_timestamp = rdtscp();

    // Push to buffer
    if (buffer.try_push(event)) {
        std::cout << "Event pushed to buffer" << std::endl;
    }

    // Pop from buffer
    MarketEvent received;
    if (buffer.try_pop(received)) {
        std::cout << "Event received from buffer" << std::endl;
        std::cout << "  Venue ID: " << received.venue_id << std::endl;
        std::cout << "  Sequence: " << received.sequence_number << std::endl;
        std::cout << "  Symbol: " << std::string(received.symbol.data, 8) << std::endl;
        std::cout << "  Price: $" << (received.price / 100000000.0) << std::endl;
        std::cout << "  Quantity: " << (received.quantity / 100000000) << std::endl;
        std::cout << "  Side: " << (received.side == Side::BID ? "BID" : "ASK") << std::endl;
    }

    std::cout << "\n=== Example Complete ===" << std::endl;
    return 0;
}
