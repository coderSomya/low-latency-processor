# Unit tests
enable_testing()

# Test executable
add_executable(orderbook_tests
    test_orderbook.cpp
    test_csv_parser.cpp
    test_processor.cpp
)

target_link_libraries(orderbook_tests
    orderbook_core
    gtest
    gtest_main
    Threads::Threads
)

# Add tests
add_test(NAME OrderbookTests COMMAND orderbook_tests) 