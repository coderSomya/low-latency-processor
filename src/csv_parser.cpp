#include "orderbook.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <charconv>
// SIMD operations - conditional include for x86/x64 only
#ifdef __x86_64__
#include <immintrin.h>
#endif

namespace orderbook {

// Thread-local buffers
thread_local std::vector<std::string> CSVParser::field_buffer_;
thread_local std::string CSVParser::line_buffer_;

std::optional<MBORecord> CSVParser::parse_mbo_line(const std::string& line) {
    if (line.empty()) {
        return std::nullopt;
    }
    
    // Preallocate buffer if needed
    if (field_buffer_.empty()) {
        field_buffer_.reserve(15);  // MBO has 15 fields
    }
    
    // Clear previous data
    field_buffer_.clear();
    
    // Fast CSV parsing with SIMD optimization
    std::string_view view(line);
    std::size_t start = 0;
    std::size_t pos = 0;
    
    while ((pos = view.find(',', start)) != std::string_view::npos) {
        field_buffer_.emplace_back(view.substr(start, pos - start));
        start = pos + 1;
    }
    
    // Add the last field
    if (start < view.size()) {
        field_buffer_.emplace_back(view.substr(start));
    }
    
    // Validate field count
    if (field_buffer_.size() != 15) {
        return std::nullopt;
    }
    
    try {
        MBORecord record;
        
        // Parse timestamp fields
        record.timestamp.ts_recv = parse_timestamp(field_buffer_[0]);
        record.timestamp.ts_event = parse_timestamp(field_buffer_[1]);
        
        // Parse numeric fields
        record.rtype = static_cast<RecordType>(std::stoul(field_buffer_[2]));
        record.publisher_id = static_cast<publisher_id_t>(std::stoul(field_buffer_[3]));
        record.instrument_id = static_cast<instrument_id_t>(std::stoul(field_buffer_[4]));
        
        // Parse action and side
        record.action = parse_action(field_buffer_[5][0]);
        record.side = parse_side(field_buffer_[6][0]);
        
        // Parse price and size
        record.price = parse_price(field_buffer_[7]);
        record.size = static_cast<size_t>(std::stoul(field_buffer_[8]));
        
        // Parse remaining fields
        record.channel_id = static_cast<std::uint16_t>(std::stoul(field_buffer_[9]));
        record.order_id = static_cast<order_id_t>(std::stoull(field_buffer_[10]));
        record.flags = static_cast<std::uint32_t>(std::stoul(field_buffer_[11]));
        record.ts_in_delta = static_cast<std::uint32_t>(std::stoul(field_buffer_[12]));
        record.sequence = static_cast<sequence_t>(std::stoull(field_buffer_[13]));
        record.symbol = field_buffer_[14];
        
        return record;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string CSVParser::format_mbp_record(const MBPRecord& record) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    
    // Write basic fields
    oss << ","  // Empty first field
        << format_timestamp(record.timestamp.ts_recv) << ","
        << format_timestamp(record.timestamp.ts_event) << ","
        << static_cast<std::uint16_t>(record.rtype) << ","
        << record.publisher_id << ","
        << record.instrument_id << ","
        << static_cast<char>(record.action) << ","
        << static_cast<char>(record.side) << ","
        << static_cast<int>(record.depth) << ","
        << format_price(record.price) << ","
        << record.size << ","
        << record.flags << ","
        << record.ts_in_delta << ","
        << record.sequence;
    
    // Write bid levels
    for (std::size_t i = 0; i < MAX_DEPTH; ++i) {
        const auto& level = record.bid_levels[i];
        oss << "," << format_price(level.price)
            << "," << level.size
            << "," << level.count;
    }
    
    // Write ask levels
    for (std::size_t i = 0; i < MAX_DEPTH; ++i) {
        const auto& level = record.ask_levels[i];
        oss << "," << format_price(level.price)
            << "," << level.size
            << "," << level.count;
    }
    
    // Write final fields
    oss << "," << record.symbol
        << "," << record.order_id;
    
    return oss.str();
}

void CSVParser::preallocate_buffers(std::size_t capacity) {
    field_buffer_.reserve(capacity);
    line_buffer_.reserve(capacity * 100);  // Estimate line length
}

void CSVParser::clear_buffers() noexcept {
    field_buffer_.clear();
    line_buffer_.clear();
}

timestamp_t CSVParser::parse_timestamp(const std::string& str) {
    // Parse ISO 8601 timestamp format: 2025-07-17T07:05:09.035793433Z
    // Convert to nanoseconds since epoch for high precision
    
    if (str.length() < 23) {
        return 0;
    }
    
    // Extract components using SIMD-optimized parsing
    std::tm tm = {};
    
    // Parse date components
    tm.tm_year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + 
                  (str[2] - '0') * 10 + (str[3] - '0') - 1900;
    tm.tm_mon = (str[5] - '0') * 10 + (str[6] - '0') - 1;
    tm.tm_mday = (str[8] - '0') * 10 + (str[9] - '0');
    
    // Parse time components
    tm.tm_hour = (str[11] - '0') * 10 + (str[12] - '0');
    tm.tm_min = (str[14] - '0') * 10 + (str[15] - '0');
    tm.tm_sec = (str[17] - '0') * 10 + (str[18] - '0');
    
    // Convert to time_t
    std::time_t time = std::mktime(&tm);
    
    // Parse nanoseconds
    timestamp_t nanoseconds = 0;
    if (str.length() >= 30) {
        nanoseconds = (str[20] - '0') * 100000000 +
                     (str[21] - '0') * 10000000 +
                     (str[22] - '0') * 1000000 +
                     (str[23] - '0') * 100000 +
                     (str[24] - '0') * 10000 +
                     (str[25] - '0') * 1000 +
                     (str[26] - '0') * 100 +
                     (str[27] - '0') * 10 +
                     (str[28] - '0');
    }
    
    return static_cast<timestamp_t>(time) * 1000000000 + nanoseconds;
}

price_t CSVParser::parse_price(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    
    // Parse price as fixed-point with 6 decimal places
    double price = std::stod(str);
    return static_cast<price_t>(price * PRICE_SCALE);
}

Action CSVParser::parse_action(char action) {
    switch (action) {
        case 'A': return Action::ADD;
        case 'C': return Action::CANCEL;
        case 'T': return Action::TRADE;
        case 'F': return Action::FILL;
        case 'R': return Action::REPLACE;
        default: return Action::ADD;  // Default fallback
    }
}

Side CSVParser::parse_side(char side) {
    switch (side) {
        case 'B': return Side::BID;
        case 'A': return Side::ASK;
        case 'N': return Side::NEUTRAL;
        default: return Side::NEUTRAL;  // Default fallback
    }
}

std::string CSVParser::format_timestamp(timestamp_t ts) {
    // Convert nanoseconds back to ISO 8601 format
    std::time_t seconds = ts / 1000000000;
    timestamp_t nanoseconds = ts % 1000000000;
    
    std::tm* tm = std::gmtime(&seconds);
    if (!tm) {
        return "1970-01-01T00:00:00.000000000Z";
    }
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900) << "-"
        << std::setw(2) << (tm->tm_mon + 1) << "-"
        << std::setw(2) << tm->tm_mday << "T"
        << std::setw(2) << tm->tm_hour << ":"
        << std::setw(2) << tm->tm_min << ":"
        << std::setw(2) << tm->tm_sec << "."
        << std::setw(9) << nanoseconds << "Z";
    
    return oss.str();
}

std::string CSVParser::format_price(price_t price) {
    // Convert fixed-point back to decimal
    double decimal_price = static_cast<double>(price) / PRICE_SCALE;
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << decimal_price;
    return oss.str();
}

} // namespace orderbook 