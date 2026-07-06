# Matching Engine

A high-performance order matching engine written in C++17. Simulates the core of a stock exchange — receiving buy/sell orders, matching them by price-time priority, and generating trades.

## Performance

| Orders | Trades | Avg Latency | Throughput |
|--------|--------|-------------|------------|
| 1,000 | 774 | 0.58 μs | 476K orders/sec |
| 100,000 | 77,969 | 0.43 μs | 1.44M orders/sec |
| 1,000,000 | 777,781 | 0.42 μs | 1.46M orders/sec |

Sub-microsecond latency, stable throughput across three orders of magnitude.

## Features

- **Price-time priority matching** — highest bid and lowest ask matched first; ties broken by arrival time (FIFO)
- **Limit orders** — buy and sell with specified price
- **Partial fills** — large orders matched across multiple counterparties
- **Order cancellation** — remove pending orders by ID
- **Multi-symbol support** — independent order books per symbol
- **Random order simulator** — generate thousands to millions of orders with latency and throughput benchmarks
- **Interactive CLI** — place orders, view the book, and run simulations in real time

## Build

Requires CMake 3.14+ and a C++17 compiler.

```bash
cmake -B build
cmake --build build
```

## Run

```bash
./build/engine
```

```
> BUY AMD 100 145.20
[ORDER] id=1 BUY 100 AMD @ 145.20

> SELL AMD 60 145.00
[ORDER] id=2 SELL 60 AMD @ 145.00
[TRADE] 60 AMD @ 145.20 | Buyer: Order#1 Seller: Order#2

> BOOK
=== Order Book ===
  --------------------
  BID 145.20 x 40

> SIM NVDA 1000000
=== Simulation Complete ===
  Orders:     1000000
  Trades:     777781
  Avg latency: 0.42 us/order
  Throughput:  1463905 orders/sec
```

## Test

11 unit tests covering matching logic, price/time priority, partial fills, cancellation, and trade pricing.

```bash
cd build && ctest --output-on-failure
```

## Project Structure

```
matching-engine/
├── CMakeLists.txt
├── include/
│   ├── Order.h            # Order and Side definitions
│   └── OrderBook.h        # OrderBook class and Trade struct
├── src/
│   ├── OrderBook.cpp       # Matching and order management logic
│   └── main.cpp            # Interactive CLI and simulator
├── tests/
│   └── test_orderbook.cpp  # Google Test unit tests
└── .github/workflows/
    └── ci.yml              # GitHub Actions CI
```

## Technical Details

- **Order book**: `std::map<double, std::deque<Order>>` — sorted by price, FIFO within each price level
- **Bid side**: `std::greater<double>` comparator so highest bid is always at `begin()`
- **Ask side**: default `std::less<double>` so lowest ask is always at `begin()`
- **Matching**: aggressive order walks the opposite side of the book until fully filled or no more matching prices
- **Benchmarking**: `std::chrono::steady_clock` for per-order latency measurement

## Roadmap

- [ ] Multithreading — separate ingestion and matching threads with a thread-safe queue
- [ ] TCP networking — multiple clients connect via sockets
- [ ] Market orders
- [ ] Historical replay from CSV
