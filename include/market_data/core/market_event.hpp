#pragma once

#include "common.hpp"
#include <cstdint>

namespace market_data {

// Event types
enum class EventType : uint8_t {
    UNKNOWN = 0,
    TRADE,
    QUOTE,
    BOOK_UPDATE,
    HEARTBEAT,
    GAP_DETECTED,
    CONNECTION_STATUS
};

// Side
enum class Side : uint8_t {
    UNKNOWN = 0,
    BID,
    ASK,
    BOTH
};

// Market event structure - exactly 64 bytes for cache efficiency
struct alignas(CACHE_LINE_SIZE) MarketEvent {
    // Timestamp fields (16 bytes)
    uint64_t exchange_timestamp;  // Exchange-provided timestamp
    uint64_t receive_timestamp;   // Local receive timestamp (TSC)

    // Identification (16 bytes)
    Symbol symbol;                // 8 bytes
    uint64_t sequence_number;     // 8 bytes

    // Price and quantity (16 bytes)
    int64_t price;                // Price in fixed-point (multiply by 1e8)
    int64_t quantity;             // Quantity in fixed-point

    // Additional data (12 bytes)
    uint32_t venue_id;            // Exchange/venue identifier
    uint32_t order_id;            // Order ID if applicable
    uint32_t trade_id;            // Trade ID if applicable

    // Event metadata (4 bytes)
    EventType event_type;         // 1 byte
    Side side;                    // 1 byte
    uint8_t book_level;           // For book updates (0-based)
    uint8_t flags;                // Additional flags

    // Padding to ensure 64-byte alignment
    uint8_t _padding[60 - 48];

    MarketEvent()
        : exchange_timestamp(0)
        , receive_timestamp(0)
        , symbol()
        , sequence_number(0)
        , price(0)
        , quantity(0)
        , venue_id(0)
        , order_id(0)
        , trade_id(0)
        , event_type(EventType::UNKNOWN)
        , side(Side::UNKNOWN)
        , book_level(0)
        , flags(0)
        , _padding{}
    {}
};

static_assert(sizeof(MarketEvent) == 64, "MarketEvent must be exactly 64 bytes");
static_assert(alignof(MarketEvent) == CACHE_LINE_SIZE, "MarketEvent must be cache-line aligned");

// Connection status event
struct ConnectionStatus {
    uint32_t venue_id;
    bool connected;
    uint64_t timestamp;
    uint64_t last_sequence;
};

} // namespace market_data
