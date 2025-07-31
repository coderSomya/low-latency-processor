#pragma once

#include "types.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <functional>

namespace orderbook {

// Forward declarations
class OrderbookLevel;
class OrderbookSide;

// Internal price level structure for orderbook operations
struct OrderbookPriceLevel {
    price_t price;
    size_t total_size;
    std::uint32_t order_count;
    std::unordered_map<order_id_t, size_t> orders;
    
    OrderbookPriceLevel() noexcept : price(0), total_size(0), order_count(0) {}
};

// High-performance orderbook implementation
class Orderbook {
public:
    Orderbook();
    ~Orderbook() = default;
    
    // Non-copyable for performance
    Orderbook(const Orderbook&) = delete;
    Orderbook& operator=(const Orderbook&) = delete;
    
    // Non-moveable due to atomic members
    Orderbook(Orderbook&&) noexcept = delete;
    Orderbook& operator=(Orderbook&&) noexcept = delete;
    
    // Core orderbook operations
    void process_mbo_record(const MBORecord& record);
    MBPRecord generate_mbp_record(const MBORecord& record) const;
    
    // Performance monitoring
    PerformanceStats get_stats() const noexcept { return stats_.load(); }
    void reset_stats() noexcept { stats_ = PerformanceStats{}; }
    
    // Thread safety
    void lock() const { mutex_.lock(); }
    void unlock() const { mutex_.unlock(); }
    bool try_lock() const { return mutex_.try_lock(); }

private:
    // Lock-free orderbook sides
    std::unique_ptr<OrderbookSide> bid_side_;
    std::unique_ptr<OrderbookSide> ask_side_;
    
    // Performance statistics (atomic for thread safety)
    mutable std::atomic<PerformanceStats> stats_;
    
    // Thread safety (mutable for const operations)
    mutable std::shared_mutex mutex_;
    
    // Internal helper methods
    void handle_add_order(const MBORecord& record);
    void handle_cancel_order(const MBORecord& record);
    void handle_trade_sequence(const MBORecord& record);
    void update_stats(const MBORecord& record, duration_t processing_time);
    
    // Trade sequence tracking
    struct TradeSequence {
        order_id_t order_id;
        Side side;
        price_t price;
        size_t remaining_size;
        timestamp_t timestamp;
    };
    
    std::unordered_map<order_id_t, TradeSequence> pending_trades_;
};

// High-performance orderbook side implementation
class OrderbookSide {
public:
    OrderbookSide() noexcept = default;
    ~OrderbookSide() = default;
    
    // Non-copyable
    OrderbookSide(const OrderbookSide&) = delete;
    OrderbookSide& operator=(const OrderbookSide&) = delete;
    
    // Moveable
    OrderbookSide(OrderbookSide&&) noexcept = default;
    OrderbookSide& operator=(OrderbookSide&&) noexcept = default;
    
    // Core operations
    void add_order(order_id_t order_id, price_t price, size_t size);
    void cancel_order(order_id_t order_id, price_t price, size_t size);
    void trade_order(order_id_t order_id, price_t price, size_t size);
    
    // Query operations
    std::array<PriceLevel, MAX_DEPTH> get_top_levels() const;
    bool has_order(order_id_t order_id) const;
    size_t get_order_size(order_id_t order_id) const;
    
    // Performance
    void clear() noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    // Price-ordered map for efficient level access
    std::map<price_t, OrderbookPriceLevel, std::greater<price_t>> levels_;  // BID side (descending)
    // std::map<price_t, OrderbookPriceLevel, std::less<price_t>> levels_;  // ASK side (ascending)
    
    // Order lookup for fast cancellation
    std::unordered_map<order_id_t, std::pair<price_t, size_t>> order_lookup_;
    
    // Internal helpers
    void update_level(price_t price, order_id_t order_id, size_t size, bool is_add);
    void remove_level_if_empty(price_t price);
    void update_order_lookup(order_id_t order_id, price_t price, size_t size, bool is_add);
};

// High-performance CSV parser
class CSVParser {
public:
    CSVParser() = default;
    ~CSVParser() = default;
    
    // Parse MBO record from CSV line
    static std::optional<MBORecord> parse_mbo_line(const std::string& line);
    
    // Write MBP record to CSV format
    static std::string format_mbp_record(const MBPRecord& record);
    
    // Performance optimizations
    static void preallocate_buffers(std::size_t capacity);
    static void clear_buffers() noexcept;

private:
    // Thread-local buffers for parsing
    static thread_local std::vector<std::string> field_buffer_;
    static thread_local std::string line_buffer_;
    
    // Helper methods
    static timestamp_t parse_timestamp(const std::string& str);
    static price_t parse_price(const std::string& str);
    static Action parse_action(char action);
    static Side parse_side(char side);
    static std::string format_timestamp(timestamp_t ts);
    static std::string format_price(price_t price);
};

// High-performance orderbook processor
class OrderbookProcessor {
public:
    OrderbookProcessor() = default;
    ~OrderbookProcessor() = default;
    
    // Process MBO file and generate MBP output
    void process_file(const std::string& input_file, const std::string& output_file);
    
    // Performance monitoring
    PerformanceStats get_stats() const noexcept { return orderbook_.get_stats(); }
    void reset_stats() noexcept { orderbook_.reset_stats(); }
    
    // Configuration
    void set_buffer_size(std::size_t size) noexcept { buffer_size_ = size; }
    void set_thread_count(std::size_t count) noexcept { thread_count_ = count; }

private:
    Orderbook orderbook_;
    std::size_t buffer_size_ = BUFFER_SIZE;
    std::size_t thread_count_ = 4;  // Default thread count
    
    // Processing methods
    void process_chunk(const std::vector<std::string>& lines);
    void write_mbp_record(const MBPRecord& record, std::ofstream& output);
    
    // Output buffer for processed records
    std::vector<std::string> processed_records_;
    
    // Performance optimizations
    void preallocate_buffers();
    void optimize_memory_layout();
};

} // namespace orderbook 