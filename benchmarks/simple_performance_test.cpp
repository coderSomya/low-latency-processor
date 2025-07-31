#include "orderbook.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <iomanip>

namespace orderbook {
namespace benchmark {

class SimplePerformanceTest {
public:
    static void run_orderbook_benchmarks() {
        std::cout << "Orderbook Performance Benchmarks\n";
        std::cout << "================================\n\n";
        
        // Test 1: Order processing throughput
        test_order_processing_throughput();
        
        // Test 2: MBP generation performance
        test_mbp_generation_performance();
        
        // Test 3: Add order performance
        test_add_order_performance();
        
        // Test 4: Cancel order performance
        test_cancel_order_performance();
        
        // Test 5: Memory efficiency
        test_memory_efficiency();
        
        std::cout << "\nBenchmark completed!\n";
    }
    
private:
    static void test_order_processing_throughput() {
        std::cout << "1. Order Processing Throughput Test\n";
        std::cout << "-----------------------------------\n";
        
        const std::size_t num_orders = 100000;
        std::vector<MBORecord> test_records;
        test_records.reserve(num_orders);
        
        // Generate test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
        std::uniform_int_distribution<size_t> size_dist(1, 1000);
        std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
        
        for (std::size_t i = 0; i < num_orders; ++i) {
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
            record.symbol = "PERF";
            record.channel_id = 1;
            record.flags = 0;
            record.ts_in_delta = 0;
            record.sequence = i;
            
            test_records.push_back(record);
        }
        
        // Run benchmark
        Orderbook orderbook;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (const auto& record : test_records) {
            orderbook.process_mbo_record(record);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(num_orders) / duration.count() * 1000000;
        
        std::cout << "  Orders processed: " << num_orders << "\n";
        std::cout << "  Processing time: " << duration.count() << " μs\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " orders/second\n";
        std::cout << "  Average time per order: " << std::fixed << std::setprecision(3)
                  << static_cast<double>(duration.count()) / num_orders << " μs\n\n";
    }
    
    static void test_mbp_generation_performance() {
        std::cout << "2. MBP Generation Performance Test\n";
        std::cout << "----------------------------------\n";
        
        const std::size_t num_generations = 10000;
        
        // Pre-populate orderbook
        Orderbook orderbook;
        std::random_device rd;
        std::mt19937 gen(rd());
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
            record.symbol = "PERF";
            
            orderbook.process_mbo_record(record);
        }
        
        // Create a sample record for MBP generation
        MBORecord sample_record;
        sample_record.symbol = "PERF";
        
        // Run benchmark
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (std::size_t i = 0; i < num_generations; ++i) {
            orderbook.generate_mbp_record(sample_record);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(num_generations) / duration.count() * 1000000;
        
        std::cout << "  MBP records generated: " << num_generations << "\n";
        std::cout << "  Generation time: " << duration.count() << " μs\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " MBP records/second\n";
        std::cout << "  Average time per MBP: " << std::fixed << std::setprecision(3)
                  << static_cast<double>(duration.count()) / num_generations << " μs\n\n";
    }
    
    static void test_add_order_performance() {
        std::cout << "3. Add Order Performance Test\n";
        std::cout << "-----------------------------\n";
        
        const std::size_t num_orders = 100000;
        
        Orderbook orderbook;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
        std::uniform_int_distribution<size_t> size_dist(1, 1000);
        std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (std::size_t i = 0; i < num_orders; ++i) {
            MBORecord record;
            record.action = Action::ADD;
            record.side = Side::BID;
            record.price = price_dist(gen);
            record.size = size_dist(gen);
            record.order_id = id_dist(gen);
            record.symbol = "PERF";
            
            orderbook.process_mbo_record(record);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(num_orders) / duration.count() * 1000000000;
        
        std::cout << "  Orders added: " << num_orders << "\n";
        std::cout << "  Processing time: " << duration.count() << " ns\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " orders/second\n";
        std::cout << "  Average time per add: " << std::fixed << std::setprecision(3)
                  << static_cast<double>(duration.count()) / num_orders << " ns\n\n";
    }
    
    static void test_cancel_order_performance() {
        std::cout << "4. Cancel Order Performance Test\n";
        std::cout << "--------------------------------\n";
        
        const std::size_t num_cancels = 10000;
        
        // Pre-populate with orders
        Orderbook orderbook;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
        std::uniform_int_distribution<size_t> size_dist(1, 1000);
        
        for (std::size_t i = 0; i < 10000; ++i) {
            MBORecord add_record;
            add_record.action = Action::ADD;
            add_record.side = Side::BID;
            add_record.price = price_dist(gen);
            add_record.size = size_dist(gen);
            add_record.order_id = i + 1;
            add_record.symbol = "PERF";
            
            orderbook.process_mbo_record(add_record);
        }
        
        // Run cancel benchmark
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (std::size_t i = 0; i < num_cancels; ++i) {
            MBORecord cancel_record;
            cancel_record.action = Action::CANCEL;
            cancel_record.side = Side::BID;
            cancel_record.price = price_dist(gen);
            cancel_record.size = size_dist(gen);
            cancel_record.order_id = (i % 10000) + 1;
            cancel_record.symbol = "PERF";
            
            orderbook.process_mbo_record(cancel_record);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(num_cancels) / duration.count() * 1000000000;
        
        std::cout << "  Orders cancelled: " << num_cancels << "\n";
        std::cout << "  Processing time: " << duration.count() << " ns\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " cancels/second\n";
        std::cout << "  Average time per cancel: " << std::fixed << std::setprecision(3)
                  << static_cast<double>(duration.count()) / num_cancels << " ns\n\n";
    }
    
    static void test_memory_efficiency() {
        std::cout << "5. Memory Efficiency Test\n";
        std::cout << "-------------------------\n";
        
        const std::size_t num_orders = 50000;
        
        Orderbook orderbook;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<price_t> price_dist(900000, 1100000);
        std::uniform_int_distribution<size_t> size_dist(1, 1000);
        std::uniform_int_distribution<order_id_t> id_dist(1, 1000000);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (std::size_t i = 0; i < num_orders; ++i) {
            MBORecord record;
            record.action = Action::ADD;
            record.side = (i % 2 == 0) ? Side::BID : Side::ASK;
            record.price = 1000000 + (i % 100) * 1000;
            record.size = 100;
            record.order_id = i + 1;
            record.symbol = "MEM";
            
            orderbook.process_mbo_record(record);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(num_orders) / duration.count() * 1000000;
        
        std::cout << "  Orders processed: " << num_orders << "\n";
        std::cout << "  Processing time: " << duration.count() << " μs\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " orders/second\n";
        std::cout << "  Memory efficiency: " << std::fixed << std::setprecision(3)
                  << static_cast<double>(duration.count()) / num_orders << " μs per order\n\n";
    }
};

} // namespace benchmark
} // namespace orderbook

int main() {
    orderbook::benchmark::SimplePerformanceTest::run_orderbook_benchmarks();
    return 0;
} 