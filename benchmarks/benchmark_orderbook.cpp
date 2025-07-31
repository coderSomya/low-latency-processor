#include "orderbook.hpp"
#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <thread>

namespace orderbook {
namespace benchmark {

// Benchmark fixture for orderbook tests
class OrderbookBenchmark : public ::benchmark::Fixture {
protected:
    void SetUp(const ::benchmark::State& state) override {
        orderbook_ = std::make_unique<Orderbook>();
        
        // Generate test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
        std::uniform_int_distribution<size_t> size_dist(1, 1000);
        std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
        
        test_records_.reserve(state.range(0));
        for (std::size_t i = 0; i < state.range(0); ++i) {
            MBORecord record;
            record.timestamp.ts_recv = i * 1000;
            record.timestamp.ts_event = i * 1000;
            record.rtype = RecordType::MBO;
            record.publisher_id = 2;
            record.instrument_id = 1108;
            record.action = (i % 3 == 0) ? Action::ADD : Action::CANCEL;
            record.side = (i % 2 == 0) ? Side::BID : Side::ASK;
            record.price = price_dist(gen);
            record.size = size_dist(gen);
            record.order_id = id_dist(gen);
            record.symbol = "BENCH";
            record.channel_id = 1;
            record.flags = 0;
            record.ts_in_delta = 0;
            record.sequence = i;
            
            test_records_.push_back(record);
        }
    }
    
    void TearDown(const ::benchmark::State& state) override {
        orderbook_.reset();
        test_records_.clear();
    }
    
    std::unique_ptr<Orderbook> orderbook_;
    std::vector<MBORecord> test_records_;
};

// Benchmark: Order processing throughput
BENCHMARK_DEFINE_F(OrderbookBenchmark, OrderProcessing)(::benchmark::State& state) {
    for (auto _ : state) {
        for (const auto& record : test_records_) {
            orderbook_->process_mbo_record(record);
        }
        orderbook_->reset_stats();
    }
    
    state.SetItemsProcessed(state.iterations() * test_records_.size());
    state.SetComplexityN(test_records_.size());
}

BENCHMARK_REGISTER_F(OrderbookBenchmark, OrderProcessing)
    ->RangeMultiplier(10)
    ->Range(100, 100000)
    ->Complexity(::benchmark::oN)
    ->Unit(::benchmark::kMicrosecond);

// Benchmark: MBP record generation
BENCHMARK_DEFINE_F(OrderbookBenchmark, MBPGeneration)(::benchmark::State& state) {
    // Pre-populate orderbook
    for (const auto& record : test_records_) {
        orderbook_->process_mbo_record(record);
    }
    
    for (auto _ : state) {
        for (const auto& record : test_records_) {
            ::benchmark::DoNotOptimize(orderbook_->generate_mbp_record(record));
        }
    }
    
    state.SetItemsProcessed(state.iterations() * test_records_.size());
    state.SetComplexityN(test_records_.size());
}

BENCHMARK_REGISTER_F(OrderbookBenchmark, MBPGeneration)
    ->RangeMultiplier(10)
    ->Range(100, 100000)
    ->Complexity(::benchmark::oN)
    ->Unit(::benchmark::kMicrosecond);

// Benchmark: Add order performance
BENCHMARK_DEFINE_F(OrderbookBenchmark, AddOrder)(::benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
    std::uniform_int_distribution<size_t> size_dist(1, 1000);
    std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
    
    for (auto _ : state) {
        MBORecord record;
        record.action = Action::ADD;
        record.side = Side::BID;
        record.price = price_dist(gen);
        record.size = size_dist(gen);
        record.order_id = id_dist(gen);
        record.symbol = "BENCH";
        
        orderbook_->process_mbo_record(record);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(OrderbookBenchmark, AddOrder)
    ->Unit(::benchmark::kNanosecond);

// Benchmark: Cancel order performance
BENCHMARK_DEFINE_F(OrderbookBenchmark, CancelOrder)(::benchmark::State& state) {
    // Pre-populate with orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
    std::uniform_int_distribution<size_t> size_dist(1, 1000);
    
    for (std::size_t i = 0; i < 1000; ++i) {
        MBORecord add_record;
        add_record.action = Action::ADD;
        add_record.side = Side::BID;
        add_record.price = price_dist(gen);
        add_record.size = size_dist(gen);
        add_record.order_id = i + 1;
        add_record.symbol = "BENCH";
        
        orderbook_->process_mbo_record(add_record);
    }
    
    for (auto _ : state) {
        MBORecord cancel_record;
        cancel_record.action = Action::CANCEL;
        cancel_record.side = Side::BID;
        cancel_record.price = price_dist(gen);
        cancel_record.size = size_dist(gen);
        cancel_record.order_id = (state.iterations() % 1000) + 1;
        cancel_record.symbol = "BENCH";
        
        orderbook_->process_mbo_record(cancel_record);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(1);
}

BENCHMARK_REGISTER_F(OrderbookBenchmark, CancelOrder)
    ->Unit(::benchmark::kNanosecond);

// Benchmark: Memory efficiency
BENCHMARK_DEFINE_F(OrderbookBenchmark, MemoryEfficiency)(::benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
    std::uniform_int_distribution<size_t> size_dist(1, 1000);
    std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
    
    for (auto _ : state) {
        // Process orders
        for (std::size_t i = 0; i < state.range(0); ++i) {
            MBORecord record;
            record.action = Action::ADD;
            record.side = (i % 2 == 0) ? Side::BID : Side::ASK;
            record.price = price_dist(gen);
            record.size = size_dist(gen);
            record.order_id = id_dist(gen);
            record.symbol = "BENCH";
            
            orderbook_->process_mbo_record(record);
        }
        
        // Reset for next iteration
        orderbook_->reset_stats();
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetComplexityN(state.range(0));
}

BENCHMARK_REGISTER_F(OrderbookBenchmark, MemoryEfficiency)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Complexity(::benchmark::oN)
    ->Unit(::benchmark::kMicrosecond);

// Benchmark: Thread safety (simulated)
BENCHMARK_DEFINE_F(OrderbookBenchmark, ThreadSafety)(::benchmark::State& state) {
    std::vector<std::thread> threads;
    std::atomic<std::size_t> counter{0};
    
    for (auto _ : state) {
        // Simulate concurrent access
        for (std::size_t t = 0; t < 4; ++t) {
            threads.emplace_back([this, &counter, t]() {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
                std::uniform_int_distribution<size_t> size_dist(1, 1000);
                std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
                
                for (std::size_t i = 0; i < 1000; ++i) {
                    MBORecord record;
                    record.action = Action::ADD;
                    record.side = (i % 2 == 0) ? Side::BID : Side::ASK;
                    record.price = price_dist(gen);
                    record.size = size_dist(gen);
                    record.order_id = id_dist(gen);
                    record.symbol = "BENCH";
                    
                    orderbook_->process_mbo_record(record);
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        threads.clear();
    }
    
    state.SetItemsProcessed(counter.load());
    state.SetComplexityN(4000); // 4 threads * 1000 orders each
}

BENCHMARK_REGISTER_F(OrderbookBenchmark, ThreadSafety)
    ->Unit(::benchmark::kMicrosecond);

} // namespace benchmark
} // namespace orderbook

BENCHMARK_MAIN(); 