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
struct alignas(64) MarketEvent {
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

    // Padding to reach exactly 64 bytes
    // Current size: 8+8+8+8+8+8+4+4+4+1+1+1+1 = 64 bytes
    // No padding needed as we're already at 64 bytes

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
    {}
};

static_assert(sizeof(MarketEvent) == 64, "MarketEvent must be exactly 64 bytes");
static_assert(alignof(MarketEvent) == 64, "MarketEvent must be 64-byte aligned");

// Connection status event
struct ConnectionStatus {
    uint32_t venue_id;
    bool connected;
    uint64_t timestamp;
    uint64_t last_sequence;
};

} // namespace market_data
