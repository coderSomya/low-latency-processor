# Performance benchmarks
add_executable(orderbook_benchmarks
    benchmark_orderbook.cpp
    benchmark_csv_parser.cpp
)

target_link_libraries(orderbook_benchmarks
    orderbook_core
    benchmark::benchmark
    Threads::Threads
) 