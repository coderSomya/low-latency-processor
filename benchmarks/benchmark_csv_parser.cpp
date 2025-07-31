#include "orderbook.hpp"
#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <sstream>

namespace orderbook {
namespace benchmark {

// Benchmark fixture for CSV parser tests
class CSVParserBenchmark : public ::benchmark::Fixture {
protected:
    void SetUp(const ::benchmark::State& state) override {
        // Generate test CSV lines
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
        std::uniform_int_distribution<size_t> size_dist(1, 1000);
        std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
        
        test_lines_.reserve(state.range(0));
        for (std::size_t i = 0; i < state.range(0); ++i) {
            std::ostringstream oss;
            oss << i * 1000 << ","  // ts_recv
                << i * 1000 << ","  // ts_event
                << "160,"            // rtype
                << "2,"              // publisher_id
                << "1108,"           // instrument_id
                << "A,"              // action
                << "B,"              // side
                << price_dist(gen) << ","  // price
                << size_dist(gen) << ","   // size
                << "1,"              // channel_id
                << id_dist(gen) << ","     // order_id
                << "0,"              // flags
                << "0,"              // ts_in_delta
                << i << ","          // sequence
                << "BENCH";          // symbol
            
            test_lines_.push_back(oss.str());
        }
    }
    
    void TearDown(const ::benchmark::State& state) override {
        test_lines_.clear();
    }
    
    std::vector<std::string> test_lines_;
};

// Benchmark: CSV parsing throughput
BENCHMARK_DEFINE_F(CSVParserBenchmark, ParseThroughput)(::benchmark::State& state) {
    for (auto _ : state) {
        for (const auto& line : test_lines_) {
            ::benchmark::DoNotOptimize(CSVParser::parse_mbo_line(line));
        }
    }
    
    state.SetItemsProcessed(state.iterations() * test_lines_.size());
    state.SetComplexityN(test_lines_.size());
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, ParseThroughput)
    ->RangeMultiplier(10)
    ->Range(100, 100000)
    ->Complexity(::benchmark::oN)
    ->Unit(::benchmark::kMicrosecond);

// Benchmark: Single line parsing
BENCHMARK_DEFINE_F(CSVParserBenchmark, SingleLineParse)(::benchmark::State& state) {
    std::string test_line = "1000,1000,160,2,1108,A,B,1000000,100,1,12345,0,0,0,BENCH";
    
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(CSVParser::parse_mbo_line(test_line));
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, SingleLineParse)
    ->Unit(::benchmark::kNanosecond);

// Benchmark: MBP record formatting
BENCHMARK_DEFINE_F(CSVParserBenchmark, MBPFormatting)(::benchmark::State& state) {
    // Create a sample MBP record
    MBPRecord record;
    record.timestamp.ts_recv = 1000;
    record.timestamp.ts_event = 1000;
    record.rtype = RecordType::MBP;
    record.publisher_id = 2;
    record.instrument_id = 1108;
    record.action = Action::ADD;
    record.side = Side::BID;
    record.depth = 0;
    record.price = 1000000;
    record.size = 100;
    record.flags = 0;
    record.ts_in_delta = 0;
    record.sequence = 0;
    record.symbol = "BENCH";
    record.order_id = 12345;
    
    // Fill bid levels
    for (std::size_t i = 0; i < MAX_DEPTH; ++i) {
        record.bid_levels[i] = PriceLevel(1000000 - i * 1000, 100 - i * 10, 1);
        record.ask_levels[i] = PriceLevel(1000000 + i * 1000, 100 + i * 10, 1);
    }
    
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(CSVParser::format_mbp_record(record));
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, MBPFormatting)
    ->Unit(::benchmark::kMicrosecond);

// Benchmark: String to number conversion
BENCHMARK_DEFINE_F(CSVParserBenchmark, StringToNumber)(::benchmark::State& state) {
    std::vector<std::string> numbers = {
        "1000000",
        "5500000",
        "21330000",
        "5900000",
        "10000000"
    };
    
    std::size_t index = 0;
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(std::stoull(numbers[index % numbers.size()]));
        index++;
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, StringToNumber)
    ->Unit(::benchmark::kNanosecond);

// Benchmark: Field splitting
BENCHMARK_DEFINE_F(CSVParserBenchmark, FieldSplitting)(::benchmark::State& state) {
    std::string test_line = "1000,1000,160,2,1108,A,B,1000000,100,1,12345,0,0,0,BENCH";
    
    for (auto _ : state) {
        std::vector<std::string> fields;
        std::string_view view(test_line);
        std::size_t start = 0;
        std::size_t pos = 0;
        
        while ((pos = view.find(',', start)) != std::string_view::npos) {
            fields.emplace_back(view.substr(start, pos - start));
            start = pos + 1;
        }
        
        if (start < view.size()) {
            fields.emplace_back(view.substr(start));
        }
        
        ::benchmark::DoNotOptimize(fields);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, FieldSplitting)
    ->Unit(::benchmark::kNanosecond);

// Benchmark: Memory allocation for parsing
BENCHMARK_DEFINE_F(CSVParserBenchmark, ParseMemoryAllocation)(::benchmark::State& state) {
    std::vector<std::string> test_lines;
    test_lines.reserve(state.range(0));
    
    for (std::size_t i = 0; i < state.range(0); ++i) {
        std::ostringstream oss;
        oss << i * 1000 << "," << i * 1000 << ",160,2,1108,A,B,1000000,100,1,12345,0,0,0,BENCH";
        test_lines.push_back(oss.str());
    }
    
    for (auto _ : state) {
        for (const auto& line : test_lines) {
            ::benchmark::DoNotOptimize(CSVParser::parse_mbo_line(line));
        }
    }
    
    state.SetItemsProcessed(state.iterations() * test_lines.size());
    state.SetComplexityN(test_lines.size());
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, ParseMemoryAllocation)
    ->RangeMultiplier(10)
    ->Range(100, 10000)
    ->Complexity(::benchmark::oN)
    ->Unit(::benchmark::kMicrosecond);

// Benchmark: Error handling (invalid lines)
BENCHMARK_DEFINE_F(CSVParserBenchmark, ErrorHandling)(::benchmark::State& state) {
    std::vector<std::string> invalid_lines = {
        "",  // Empty line
        "invalid",  // Too few fields
        "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16",  // Too many fields
        "1,2,3,4,5,6,7,invalid,9,10,11,12,13,14,15",  // Invalid number
        "1,2,3,4,5,X,7,8,9,10,11,12,13,14,15"  // Invalid action
    };
    
    std::size_t index = 0;
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(CSVParser::parse_mbo_line(invalid_lines[index % invalid_lines.size()]));
        index++;
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(CSVParserBenchmark, ErrorHandling)
    ->Unit(::benchmark::kNanosecond);

} // namespace benchmark
} // namespace orderbook

BENCHMARK_MAIN(); 