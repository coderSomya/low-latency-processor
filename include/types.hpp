#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <array>
#include <optional>

namespace orderbook {

// Performance-optimized integer types
using timestamp_t = std::int64_t;
using price_t = std::int64_t;  // Fixed-point arithmetic for precision
using size_t = std::uint32_t;
using order_id_t = std::uint64_t;
using sequence_t = std::uint64_t;
using instrument_id_t = std::uint32_t;
using publisher_id_t = std::uint16_t;

// Constants for performance
constexpr std::size_t MAX_DEPTH = 10;
constexpr std::size_t PRICE_SCALE = 1000000;  // 6 decimal places for price precision
constexpr std::size_t BUFFER_SIZE = 8192;      // Optimized for L1 cache

// Action types (using char for memory efficiency)
enum class Action : char {
    ADD = 'A',
    CANCEL = 'C',
    TRADE = 'T',
    FILL = 'F',
    REPLACE = 'R',
    CLEAR = 'R'  // Same as REPLACE but different context
};

// Side types
enum class Side : char {
    BID = 'B',
    ASK = 'A',
    NEUTRAL = 'N'
};

// Record types
enum class RecordType : std::uint16_t {
    MBO = 160,
    MBP = 10
};

// High-performance timestamp structure
struct Timestamp {
    timestamp_t ts_recv;
    timestamp_t ts_event;
    
    constexpr bool operator<(const Timestamp& other) const noexcept {
        return ts_event < other.ts_event;
    }
    
    constexpr bool operator==(const Timestamp& other) const noexcept {
        return ts_event == other.ts_event;
    }
};

// MBO record structure (aligned for cache efficiency)
struct alignas(64) MBORecord {
    Timestamp timestamp;
    RecordType rtype;
    publisher_id_t publisher_id;
    instrument_id_t instrument_id;
    Action action;
    Side side;
    price_t price;
    size_t size;
    std::uint16_t channel_id;
    order_id_t order_id;
    std::uint32_t flags;
    std::uint32_t ts_in_delta;
    sequence_t sequence;
    std::string symbol;
    
    // Default constructor for performance
    MBORecord() noexcept = default;
    
    // Move constructor for efficiency
    MBORecord(MBORecord&&) noexcept = default;
    MBORecord& operator=(MBORecord&&) noexcept = default;
    
    // Copy constructor
    MBORecord(const MBORecord&) = default;
    MBORecord& operator=(const MBORecord&) = default;
};

// Price level structure for orderbook
struct alignas(32) PriceLevel {
    price_t price;
    size_t size;
    std::uint32_t count;
    
    constexpr PriceLevel() noexcept : price(0), size(0), count(0) {}
    constexpr PriceLevel(price_t p, size_t s, std::uint32_t c) noexcept 
        : price(p), size(s), count(c) {}
    
    // Copy constructor for compatibility
    constexpr PriceLevel(const PriceLevel& other) noexcept 
        : price(other.price), size(other.size), count(other.count) {}
    
    // Copy assignment operator to fix deprecation warning
    constexpr PriceLevel& operator=(const PriceLevel& other) noexcept {
        if (this != &other) {
            price = other.price;
            size = other.size;
            count = other.count;
        }
        return *this;
    }
    
    constexpr bool operator==(const PriceLevel& other) const noexcept {
        return price == other.price && size == other.size && count == other.count;
    }
    
    constexpr bool operator!=(const PriceLevel& other) const noexcept {
        return !(*this == other);
    }
};

// MBP record structure (aligned for SIMD operations)
struct alignas(128) MBPRecord {
    Timestamp timestamp;
    RecordType rtype;
    publisher_id_t publisher_id;
    instrument_id_t instrument_id;
    Action action;
    Side side;
    std::uint8_t depth;
    price_t price;
    size_t size;
    std::uint32_t flags;
    std::uint32_t ts_in_delta;
    sequence_t sequence;
    
    // Price levels for both sides (10 levels each)
    std::array<PriceLevel, MAX_DEPTH> bid_levels;
    std::array<PriceLevel, MAX_DEPTH> ask_levels;
    
    std::string symbol;
    order_id_t order_id;
    
    // Default constructor
    MBPRecord() noexcept = default;
    
    // Move constructor
    MBPRecord(MBPRecord&&) noexcept = default;
    MBPRecord& operator=(MBPRecord&&) noexcept = default;
    
    // Copy constructor
    MBPRecord(const MBPRecord&) = default;
    MBPRecord& operator=(const MBPRecord&) = default;
};

// Performance monitoring types
using duration_t = std::chrono::nanoseconds;
using time_point_t = std::chrono::high_resolution_clock::time_point;

// Statistics structure
struct PerformanceStats {
    std::size_t records_processed;
    std::size_t trades_processed;
    std::size_t orders_added;
    std::size_t orders_cancelled;
    duration_t total_processing_time;
    duration_t average_processing_time;
    
    PerformanceStats() noexcept 
        : records_processed(0), trades_processed(0), orders_added(0), 
          orders_cancelled(0), total_processing_time(0), average_processing_time(0) {}
};

} // namespace orderbook 