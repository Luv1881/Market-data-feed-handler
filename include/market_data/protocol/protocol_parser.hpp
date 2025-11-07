#pragma once

#include "../core/common.hpp"
#include "../core/market_event.hpp"
#include <string_view>
#include <cstring>
#include <optional>

namespace market_data {

/**
 * Base protocol parser interface
 */
class ProtocolParser {
public:
    virtual ~ProtocolParser() = default;

    /**
     * Parse raw data into market event
     * @param data Raw data buffer
     * @param size Size of data
     * @param[out] event Parsed market event
     * @return Number of bytes consumed, or 0 if incomplete message
     */
    virtual std::size_t parse(const char* data, std::size_t size, MarketEvent& event) = 0;

    /**
     * Get parser name
     */
    virtual const char* name() const = 0;
};

/**
 * Simple FIX protocol parser (simplified for demonstration)
 *
 * FIX message format:
 * 8=FIX.4.2|9=length|35=msgtype|...|10=checksum|
 * (| represents SOH character \x01)
 */
class FIXParser : public ProtocolParser {
public:
    explicit FIXParser(uint32_t venue_id) : venue_id_(venue_id) {}

    std::size_t parse(const char* data, std::size_t size, MarketEvent& event) override {
        if (size < 20) { // Minimum FIX message size
            return 0;
        }

        // Find message boundary (10=checksum|)
        const char* end = static_cast<const char*>(
            std::memchr(data, '\x01', size));

        if (!end) {
            return 0; // Incomplete message
        }

        std::size_t msg_size = end - data + 1;

        // Parse FIX message (simplified)
        std::string_view msg(data, msg_size);

        event.venue_id = venue_id_;
        event.receive_timestamp = rdtscp();

        // Parse message type (35=)
        auto msgtype_pos = msg.find("35=");
        if (msgtype_pos != std::string_view::npos) {
            char msgtype = msg[msgtype_pos + 3];
            switch (msgtype) {
                case 'D': event.event_type = EventType::TRADE; break;
                case 'W': event.event_type = EventType::BOOK_UPDATE; break;
                case '0': event.event_type = EventType::HEARTBEAT; break;
                default: event.event_type = EventType::UNKNOWN; break;
            }
        }

        // Parse symbol (55=)
        auto symbol_pos = msg.find("55=");
        if (symbol_pos != std::string_view::npos) {
            auto symbol_start = symbol_pos + 3;
            auto symbol_end = msg.find('\x01', symbol_start);
            if (symbol_end != std::string_view::npos) {
                auto symbol_str = msg.substr(symbol_start, symbol_end - symbol_start);
                std::memset(event.symbol.data, 0, sizeof(event.symbol.data));
                std::memcpy(event.symbol.data, symbol_str.data(),
                           std::min(symbol_str.size(), sizeof(event.symbol.data)));
            }
        }

        // Parse price (44=)
        auto price_pos = msg.find("44=");
        if (price_pos != std::string_view::npos) {
            event.price = parse_price(msg.substr(price_pos + 3));
        }

        // Parse quantity (38=)
        auto qty_pos = msg.find("38=");
        if (qty_pos != std::string_view::npos) {
            event.quantity = parse_quantity(msg.substr(qty_pos + 3));
        }

        // Parse sequence number (34=)
        auto seq_pos = msg.find("34=");
        if (seq_pos != std::string_view::npos) {
            event.sequence_number = parse_int(msg.substr(seq_pos + 3));
        }

        return msg_size;
    }

    const char* name() const override {
        return "FIX";
    }

private:
    /**
     * Parse price from string (convert to fixed-point)
     */
    int64_t parse_price(std::string_view str) const {
        // Find decimal point
        auto dot_pos = str.find('.');
        auto end_pos = str.find('\x01');

        if (dot_pos == std::string_view::npos) {
            // No decimal point
            return parse_int(str) * 100000000LL;
        }

        // Parse integer part
        int64_t integer_part = parse_int(str.substr(0, dot_pos));

        // Parse fractional part
        auto frac_start = dot_pos + 1;
        auto frac_len = (end_pos != std::string_view::npos) ?
                        (end_pos - frac_start) : (str.size() - frac_start);

        int64_t frac_part = 0;
        if (frac_len > 0 && frac_len < str.size()) {
            frac_part = parse_int(str.substr(frac_start, frac_len));
            // Adjust for decimal places
            for (std::size_t i = frac_len; i < 8; ++i) {
                frac_part *= 10;
            }
        }

        return integer_part * 100000000LL + frac_part;
    }

    /**
     * Parse quantity (similar to price)
     */
    int64_t parse_quantity(std::string_view str) const {
        return parse_price(str); // Same logic
    }

    /**
     * Parse integer from string
     */
    int64_t parse_int(std::string_view str) const {
        int64_t result = 0;
        bool negative = false;
        std::size_t i = 0;

        if (!str.empty() && str[0] == '-') {
            negative = true;
            i = 1;
        }

        for (; i < str.size() && str[i] >= '0' && str[i] <= '9'; ++i) {
            result = result * 10 + (str[i] - '0');
        }

        return negative ? -result : result;
    }

    uint32_t venue_id_;
};

/**
 * Binary protocol parser (example for fast binary formats)
 */
class BinaryParser : public ProtocolParser {
public:
    explicit BinaryParser(uint32_t venue_id) : venue_id_(venue_id) {}

    std::size_t parse(const char* data, std::size_t size, MarketEvent& event) override {
        // Example: assume binary format with fixed header
        struct MessageHeader {
            uint16_t message_length;
            uint8_t message_type;
            uint8_t reserved;
        } __attribute__((packed));

        if (size < sizeof(MessageHeader)) {
            return 0; // Incomplete header
        }

        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

        if (size < header->message_length) {
            return 0; // Incomplete message
        }

        // Parse message (simplified example)
        event.venue_id = venue_id_;
        event.receive_timestamp = rdtscp();
        event.event_type = static_cast<EventType>(header->message_type);

        // Parse rest of message based on type
        // (Implementation depends on specific protocol)

        return header->message_length;
    }

    const char* name() const override {
        return "Binary";
    }

private:
    uint32_t venue_id_;
};

/**
 * Parser factory
 */
class ParserFactory {
public:
    enum class ParserType {
        FIX,
        BINARY
    };

    static std::unique_ptr<ProtocolParser> create(ParserType type, uint32_t venue_id) {
        switch (type) {
            case ParserType::FIX:
                return std::make_unique<FIXParser>(venue_id);
            case ParserType::BINARY:
                return std::make_unique<BinaryParser>(venue_id);
            default:
                return nullptr;
        }
    }
};

} // namespace market_data
