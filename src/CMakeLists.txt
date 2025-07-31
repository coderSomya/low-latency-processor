# Core orderbook library
add_library(orderbook_core
    orderbook.cpp
    csv_parser.cpp
    processor.cpp
)

target_include_directories(orderbook_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set compile options for performance
target_compile_options(orderbook_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-march=native -mtune=native>
    $<$<CXX_COMPILER_ID:Clang>:-march=native -mtune=native>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /arch:AVX2>
)

# Link libraries
target_link_libraries(orderbook_core PUBLIC
    Threads::Threads
) 