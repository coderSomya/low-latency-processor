#include "orderbook.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
// SIMD operations - conditional include for x86/x64 only
#ifdef __x86_64__
#include <immintrin.h>
#endif

namespace orderbook {

// Orderbook implementation
Orderbook::Orderbook() 
    : bid_side_(std::make_unique<OrderbookSide>())
    , ask_side_(std::make_unique<OrderbookSide>()) {
}

void Orderbook::process_mbo_record(const MBORecord& record) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Skip initial clear action
    if (record.action == Action::CLEAR && record.sequence == 0) {
        return;
    }
    
    // Handle different action types
    switch (record.action) {
        case Action::ADD:
            handle_add_order(record);
            break;
        case Action::CANCEL:
            handle_cancel_order(record);
            break;
        case Action::TRADE:
        case Action::FILL:
            handle_trade_sequence(record);
            break;
        default:
            // Ignore other actions
            break;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<duration_t>(end_time - start_time);
    update_stats(record, processing_time);
}

MBPRecord Orderbook::generate_mbp_record(const MBORecord& record) const {
    MBPRecord mbp_record;
    
    // Copy basic fields
    mbp_record.timestamp = record.timestamp;
    mbp_record.rtype = RecordType::MBP;
    mbp_record.publisher_id = record.publisher_id;
    mbp_record.instrument_id = record.instrument_id;
    mbp_record.action = record.action;
    mbp_record.side = record.side;
    mbp_record.depth = 0;  // Will be calculated
    mbp_record.price = record.price;
    mbp_record.size = record.size;
    mbp_record.flags = record.flags;
    mbp_record.ts_in_delta = record.ts_in_delta;
    mbp_record.sequence = record.sequence;
    mbp_record.symbol = record.symbol;
    mbp_record.order_id = record.order_id;
    
    // Get top levels from both sides
    auto bid_levels = bid_side_->get_top_levels();
    auto ask_levels = ask_side_->get_top_levels();
    
    // Copy levels to output
    std::copy(bid_levels.begin(), bid_levels.end(), mbp_record.bid_levels.begin());
    std::copy(ask_levels.begin(), ask_levels.end(), mbp_record.ask_levels.begin());
    
    return mbp_record;
}

void Orderbook::handle_add_order(const MBORecord& record) {
    if (record.side == Side::BID) {
        bid_side_->add_order(record.order_id, record.price, record.size);
    } else if (record.side == Side::ASK) {
        ask_side_->add_order(record.order_id, record.price, record.size);
    }
}

void Orderbook::handle_cancel_order(const MBORecord& record) {
    if (record.side == Side::BID) {
        bid_side_->cancel_order(record.order_id, record.price, record.size);
    } else if (record.side == Side::ASK) {
        ask_side_->cancel_order(record.order_id, record.price, record.size);
    }
}

void Orderbook::handle_trade_sequence(const MBORecord& record) {
    // Handle special T->F->C sequence logic
    if (record.action == Action::TRADE) {
        // Store trade for later processing
        TradeSequence seq;
        seq.order_id = record.order_id;
        seq.side = record.side;
        seq.price = record.price;
        seq.remaining_size = record.size;
        seq.timestamp = record.timestamp.ts_event;
        
        pending_trades_[record.order_id] = seq;
    } else if (record.action == Action::FILL) {
        // Update pending trade
        auto it = pending_trades_.find(record.order_id);
        if (it != pending_trades_.end()) {
            it->second.remaining_size -= record.size;
        }
    } else if (record.action == Action::CANCEL) {
        // Process the complete T->F->C sequence
        auto it = pending_trades_.find(record.order_id);
        if (it != pending_trades_.end()) {
            const auto& seq = it->second;
            
            // Apply the trade to the opposite side
            Side opposite_side = (seq.side == Side::BID) ? Side::ASK : Side::BID;
            
            if (opposite_side == Side::BID) {
                bid_side_->trade_order(record.order_id, seq.price, seq.remaining_size);
            } else {
                ask_side_->trade_order(record.order_id, seq.price, seq.remaining_size);
            }
            
            pending_trades_.erase(it);
        }
    }
}

void Orderbook::update_stats(const MBORecord& record, duration_t processing_time) {
    PerformanceStats current_stats = stats_.load();
    
    current_stats.records_processed++;
    current_stats.total_processing_time += processing_time;
    current_stats.average_processing_time = 
        duration_t(current_stats.total_processing_time.count() / current_stats.records_processed);
    
    if (record.action == Action::TRADE) {
        current_stats.trades_processed++;
    } else if (record.action == Action::ADD) {
        current_stats.orders_added++;
    } else if (record.action == Action::CANCEL) {
        current_stats.orders_cancelled++;
    }
    
    stats_.store(current_stats);
}

// OrderbookSide implementation

void OrderbookSide::add_order(order_id_t order_id, price_t price, size_t size) {
    update_level(price, order_id, size, true);
    update_order_lookup(order_id, price, size, true);
}

void OrderbookSide::cancel_order(order_id_t order_id, price_t price, size_t size) {
    update_level(price, order_id, size, false);
    update_order_lookup(order_id, price, size, false);
}

void OrderbookSide::trade_order(order_id_t order_id, price_t /*price*/, size_t size) {
    // Find the order and reduce its size
    auto it = order_lookup_.find(order_id);
    if (it != order_lookup_.end()) {
        auto [order_price, order_size] = it->second;
        
        if (size >= order_size) {
            // Complete trade - remove order
            update_level(order_price, order_id, order_size, false);
            update_order_lookup(order_id, order_price, order_size, false);
        } else {
            // Partial trade - reduce size
            update_level(order_price, order_id, size, false);
            update_order_lookup(order_id, order_price, size, false);
        }
    }
}

std::array<PriceLevel, MAX_DEPTH> OrderbookSide::get_top_levels() const {
    std::array<PriceLevel, MAX_DEPTH> result;
    result.fill(PriceLevel{});
    
    std::size_t index = 0;
    for (const auto& [price, level] : levels_) {
        if (index >= MAX_DEPTH) break;
        
        result[index] = orderbook::PriceLevel(price, level.total_size, level.order_count);
        index++;
    }
    
    return result;
}

bool OrderbookSide::has_order(order_id_t order_id) const {
    return order_lookup_.find(order_id) != order_lookup_.end();
}

size_t OrderbookSide::get_order_size(order_id_t order_id) const {
    auto it = order_lookup_.find(order_id);
    return (it != order_lookup_.end()) ? it->second.second : 0;
}

void OrderbookSide::clear() noexcept {
    levels_.clear();
    order_lookup_.clear();
}

std::size_t OrderbookSide::size() const noexcept {
    return order_lookup_.size();
}

bool OrderbookSide::empty() const noexcept {
    return order_lookup_.empty();
}

void OrderbookSide::update_level(price_t price, order_id_t order_id, size_t size, bool is_add) {
    auto& level = levels_[price];
    level.price = price;
    
    if (is_add) {
        level.total_size += size;
        level.order_count++;
        level.orders[order_id] = size;
    } else {
        auto order_it = level.orders.find(order_id);
        if (order_it != level.orders.end()) {
            size_t order_size = order_it->second;
            level.total_size -= std::min(order_size, size);
            level.orders.erase(order_it);
            
            if (level.total_size == 0) {
                level.order_count = 0;
            } else {
                level.order_count--;
            }
        }
    }
    
    remove_level_if_empty(price);
}

void OrderbookSide::remove_level_if_empty(price_t price) {
    auto it = levels_.find(price);
    if (it != levels_.end() && it->second.total_size == 0) {
        levels_.erase(it);
    }
}

void OrderbookSide::update_order_lookup(order_id_t order_id, price_t price, size_t size, bool is_add) {
    if (is_add) {
        order_lookup_[order_id] = std::make_pair(price, size);
    } else {
        auto it = order_lookup_.find(order_id);
        if (it != order_lookup_.end()) {
            auto& [order_price, order_size] = it->second;
            if (size >= order_size) {
                order_lookup_.erase(it);
            } else {
                order_size -= size;
            }
        }
    }
}

} // namespace orderbook 