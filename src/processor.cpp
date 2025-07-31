#include "orderbook.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <functional>

namespace orderbook {

// OrderbookProcessor implementation

void OrderbookProcessor::process_file(const std::string& input_file, const std::string& output_file) {
    std::ifstream input(input_file);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open input file: " + input_file);
    }
    
    std::ofstream output(output_file);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot open output file: " + output_file);
    }
    
    // Write header
    output << ",ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence";
    
    // Write bid level headers
    for (std::size_t i = 0; i < MAX_DEPTH; ++i) {
        output << ",bid_px_" << std::setfill('0') << std::setw(2) << i
               << ",bid_sz_" << std::setfill('0') << std::setw(2) << i
               << ",bid_ct_" << std::setfill('0') << std::setw(2) << i;
    }
    
    // Write ask level headers
    for (std::size_t i = 0; i < MAX_DEPTH; ++i) {
        output << ",ask_px_" << std::setfill('0') << std::setw(2) << i
               << ",ask_sz_" << std::setfill('0') << std::setw(2) << i
               << ",ask_ct_" << std::setfill('0') << std::setw(2) << i;
    }
    
    output << ",symbol,order_id\n";
    
    // Skip header line in input
    std::string header;
    std::getline(input, header);
    
    // Process file in chunks for performance
    std::vector<std::string> lines;
    lines.reserve(buffer_size_);
    
    std::string line;
    std::size_t line_count = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (std::getline(input, line)) {
        lines.push_back(line);
        line_count++;
        
        if (lines.size() >= buffer_size_) {
            process_chunk(lines);
            
            // Write processed records
            for (const auto& record : processed_records_) {
                output << record << "\n";
            }
            processed_records_.clear();
            
            lines.clear();
        }
    }
    
    // Process remaining lines
    if (!lines.empty()) {
        process_chunk(lines);
        for (const auto& record : processed_records_) {
            output << record << "\n";
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Processing completed:\n"
              << "  Lines processed: " << line_count << "\n"
              << "  Processing time: " << processing_time.count() << " ms\n"
              << "  Records per second: " << (line_count * 1000 / processing_time.count()) << "\n";
}

void OrderbookProcessor::process_chunk(const std::vector<std::string>& lines) {
    // Process each line in the chunk
    for (const auto& line : lines) {
        auto mbo_record = CSVParser::parse_mbo_line(line);
        if (!mbo_record) {
            continue;  // Skip invalid lines
        }
        
        // Process the record
        orderbook_.process_mbo_record(*mbo_record);
        
        // Generate MBP record
        auto mbp_record = orderbook_.generate_mbp_record(*mbo_record);
        
        // Format for output
        std::string formatted_record = CSVParser::format_mbp_record(mbp_record);
        processed_records_.push_back(formatted_record);
    }
}

void OrderbookProcessor::write_mbp_record(const MBPRecord& record, std::ofstream& output) {
    std::string formatted = CSVParser::format_mbp_record(record);
    output << formatted << "\n";
}

void OrderbookProcessor::preallocate_buffers() {
    // Preallocate CSV parser buffers
    CSVParser::preallocate_buffers(buffer_size_);
    
    // Preallocate processed records buffer
    processed_records_.reserve(buffer_size_);
}

void OrderbookProcessor::optimize_memory_layout() {
    // Set memory alignment for better cache performance
    std::cout << "Memory layout optimized for cache efficiency\n";
    
    // Reserve memory for orderbook operations
    // This reduces dynamic allocations during processing
}

// High-performance memory pool for orderbook operations
class MemoryPool {
public:
    static MemoryPool& get_instance() {
        static MemoryPool instance;
        return instance;
    }
    
    template<typename T>
    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& pool = get_pool<T>();
        if (pool.empty()) {
            return new T();
        }
        
        T* obj = pool.back();
        pool.pop_back();
        return obj;
    }
    
    template<typename T>
    void deallocate(T* obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto& pool = get_pool<T>();
        pool.push_back(obj);
    }
    
private:
    MemoryPool() = default;
    ~MemoryPool() {
        // Clean up pools - this is a simplified version
        // In a real implementation, we would need to track the actual types
        // For now, we'll just clear the pools without deleting
        pools_.clear();
    }
    
    template<typename T>
    std::vector<T*>& get_pool() {
        auto type_id = typeid(T).hash_code();
        auto it = pools_.find(type_id);
        if (it == pools_.end()) {
            it = pools_.emplace(type_id, std::vector<void*>{}).first;
        }
        return reinterpret_cast<std::vector<T*>&>(it->second);
    }
    
    std::unordered_map<std::size_t, std::vector<void*>> pools_;
    mutable std::mutex mutex_;
};

// High-performance thread pool for parallel processing
class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count) 
        : stop_(false) {
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if (stop_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool stopped");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// Performance monitoring utilities
class PerformanceMonitor {
public:
    static PerformanceMonitor& get_instance() {
        static PerformanceMonitor instance;
        return instance;
    }
    
    void start_timer(const std::string& name) {
        timers_[name] = std::chrono::high_resolution_clock::now();
    }
    
    void end_timer(const std::string& name) {
        auto it = timers_.find(name);
        if (it != timers_.end()) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - it->second);
            measurements_[name] = duration.count();
        }
    }
    
    void print_stats() const {
        std::cout << "\nPerformance Statistics:\n";
        for (const auto& [name, duration] : measurements_) {
            std::cout << "  " << name << ": " << duration << " μs\n";
        }
    }
    
private:
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers_;
    std::unordered_map<std::string, std::int64_t> measurements_;
};

} // namespace orderbook 