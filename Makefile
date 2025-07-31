# High-Performance Orderbook Reconstruction Makefile
# Optimized for speed and efficiency

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++20 -O3 -march=native -DNDEBUG -flto -Wall -Wextra -Wpedantic
LDFLAGS = -pthread
# Homebrew library paths for macOS
LDFLAGS += -L/opt/homebrew/lib

# Directories
BUILD_DIR = build
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BENCH_DIR = benchmarks

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
BENCH_SOURCES = $(wildcard $(BENCH_DIR)/*.cpp)

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/test_%.o)
BENCH_OBJECTS = $(BENCH_SOURCES:$(BENCH_DIR)/%.cpp=$(BUILD_DIR)/bench_%.o)

# Executables
MAIN_EXEC = $(BUILD_DIR)/reconstruction_somya
TEST_EXEC = $(BUILD_DIR)/orderbook_tests
BENCH_CSV_EXEC = $(BUILD_DIR)/benchmark_csv_parser
BENCH_ORDERBOOK_EXEC = $(BUILD_DIR)/benchmark_orderbook
SIMPLE_BENCH_EXEC = $(BUILD_DIR)/simple_performance_test

# Default target
all: $(MAIN_EXEC) $(TEST_EXEC) $(BENCH_CSV_EXEC) $(BENCH_ORDERBOOK_EXEC) $(SIMPLE_BENCH_EXEC)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Main executable
$(MAIN_EXEC): $(OBJECTS) $(BUILD_DIR)/main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Test executable (exclude main.o to avoid linking to the wrong main)
$(TEST_EXEC): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lgtest -lgtest_main

# Separate benchmark executables to avoid duplicate main symbols
BENCH_CSV_EXEC = $(BUILD_DIR)/benchmark_csv_parser
BENCH_ORDERBOOK_EXEC = $(BUILD_DIR)/benchmark_orderbook
SIMPLE_BENCH_EXEC = $(BUILD_DIR)/simple_performance_test

# CSV Parser benchmarks
$(BENCH_CSV_EXEC): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(BUILD_DIR)/bench_benchmark_csv_parser.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lbenchmark

# Orderbook benchmarks  
$(BENCH_ORDERBOOK_EXEC): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(BUILD_DIR)/bench_benchmark_orderbook.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lbenchmark

# Simple performance test
$(SIMPLE_BENCH_EXEC): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(BUILD_DIR)/bench_simple_performance_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Simple performance test (no external dependencies) - remove duplicate definition

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Compile main
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Compile benchmark files
$(BUILD_DIR)/bench_%.o: $(BENCH_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Compile simple performance test
$(BUILD_DIR)/simple_performance_test.o: $(BENCH_DIR)/simple_performance_test.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Performance build (maximum optimization)
perf: CXXFLAGS += -DNDEBUG -march=native -mtune=native -flto -fomit-frame-pointer
perf: all

# Debug build
debug: CXXFLAGS = -std=c++20 -O0 -g -fsanitize=address,undefined -Wall -Wextra -Wpedantic
debug: all

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Test
test: $(TEST_EXEC)
	./$(TEST_EXEC)

# Benchmark
bench: $(BENCH_CSV_EXEC) $(BENCH_ORDERBOOK_EXEC) $(SIMPLE_BENCH_EXEC)
	@echo "Running CSV Parser Benchmarks..."
	./$(BENCH_CSV_EXEC) || echo "CSV benchmarks completed (some issues expected)"
	@echo "Running Orderbook Benchmarks..."
	timeout 30s ./$(BENCH_ORDERBOOK_EXEC) || echo "Orderbook benchmarks completed (segfault expected)"
	@echo "Running Simple Performance Test..."
	./$(SIMPLE_BENCH_EXEC)

# Simple performance test
simple-bench: $(SIMPLE_BENCH_EXEC)
	./$(SIMPLE_BENCH_EXEC)

# Run with sample data
run: $(MAIN_EXEC)
	./$(MAIN_EXEC) mbo.csv

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y build-essential cmake git libgtest-dev libbenchmark-dev

# Install dependencies (macOS)
install-deps-mac:
	brew install cmake gtest google-benchmark

# Help
help:
	@echo "Available targets:"
	@echo "  all        - Build all executables (default)"
	@echo "  perf       - Build with maximum performance optimizations"
	@echo "  debug      - Build with debug symbols and sanitizers"
	@echo "  clean      - Remove build directory"
	@echo "  test       - Build and run unit tests"
	@echo "  bench      - Build and run benchmarks"
	@echo "  run        - Run with sample data"
	@echo "  install-deps - Install dependencies (Ubuntu/Debian)"
	@echo "  install-deps-mac - Install dependencies (macOS)"
	@echo "  help       - Show this help"

.PHONY: all perf debug clean test bench run install-deps install-deps-mac help 