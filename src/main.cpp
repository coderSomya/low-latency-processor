#include "orderbook.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <input_mbo_file.csv>\n";
            std::cerr << "Example: " << argv[0] << " mbo.csv\n";
            return 1;
        }
        
        std::string input_file = argv[1];
        std::string output_file = "output_mbp.csv";
        
        std::cout << "High-Performance Orderbook Reconstruction\n";
        std::cout << "========================================\n";
        std::cout << "Input file: " << input_file << "\n";
        std::cout << "Output file: " << output_file << "\n";
        std::cout << "Processing...\n\n";
        
        // Create processor with optimized settings
        orderbook::OrderbookProcessor processor;
        
        // Set performance parameters
        processor.set_buffer_size(16384);  // Larger buffer for better performance
        processor.set_thread_count(std::thread::hardware_concurrency());
        
        // Start performance monitoring
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Process the file
        processor.process_file(input_file, output_file);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Get performance statistics
        const auto stats = processor.get_stats();
        
        // Print results
        std::cout << "\nProcessing Results:\n";
        std::cout << "==================\n";
        std::cout << "Total processing time: " << total_time.count() << " ms\n";
        std::cout << "Records processed: " << stats.records_processed << "\n";
        std::cout << "Trades processed: " << stats.trades_processed << "\n";
        std::cout << "Orders added: " << stats.orders_added << "\n";
        std::cout << "Orders cancelled: " << stats.orders_cancelled << "\n";
        std::cout << "Average processing time: " << stats.average_processing_time.count() << " ns\n";
        
        if (stats.records_processed > 0) {
            double throughput = (stats.records_processed * 1000.0) / total_time.count();
            std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                      << throughput << " records/second\n";
        }
        
        std::cout << "\nOutput written to: " << output_file << "\n";
        std::cout << "Processing completed successfully!\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
} 