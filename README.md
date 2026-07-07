# Matching Engine

A high-performance order matching engine written in C++17. Simulates the core of a stock exchange — receiving buy/sell orders, matching them by price-time priority, and generating trades.

Supports standalone interactive mode, multi-threaded simulation, and networked multi-client trading over TCP.

![Multi-client trading demo](docs/demo.png)

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
- **Multi-symbol support** — independent order books per symbol (AMD, NVDA, TSM, etc.)
- **Random order simulator** — generate up to 10M orders with latency and throughput benchmarks
- **Multi-threaded simulation** — separate ingestion and matching threads connected by a thread-safe queue
- **TCP server/client** — multiple traders connect over the network and trade against each other in real time
- **Interactive CLI** — place orders, view the book, and run simulations

## Build

Requires CMake 3.14+ and a C++17 compiler.

```bash
cmake -B build
cmake --build build
```

This produces three executables: `engine` (standalone), `server`, and `client`.

## Usage

### Standalone Mode

```bash
./build/engine
```

```
> BUY AMD 100 145.20
[ORDER] id=1 BUY 100 AMD @ 145.20

> SELL AMD 60 145.00
[TRADE] 60 AMD @ 145.20 | Buyer: Order#1 Seller: Order#2

> BOOK
=== Order Book ===
  --------------------
  BID 145.20 x 40

> SIM NVDA 1000000
=== Single-Thread Simulation ===
  Orders:     1000000
  Trades:     777781
  Avg latency: 0.42 us/order
  Throughput:  1463905 orders/sec

> MSIM NVDA 1000000
=== Multi-Thread Simulation ===
  Orders:     1000000
  Trades:     777781
  Throughput:  1060634 orders/sec
  (Ingestion and matching ran on separate threads)
```

### Networked Mode

Start the server:

```bash
./build/server 9000
```

Connect clients from separate terminals:

```bash
./build/client localhost 9000
```

Multiple clients can trade against each other in real time. Each client gets an independent session, and orders are matched across all connected traders.

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
│   ├── Order.h              # Order and Side definitions
│   ├── OrderBook.h          # OrderBook class and Trade struct
│   ├── ThreadSafeQueue.h    # Lock-based concurrent queue
│   └── Server.h             # TCP server class
├── src/
│   ├── OrderBook.cpp         # Matching and order management logic
│   ├── main.cpp              # Standalone CLI and simulators
│   ├── Server.cpp            # TCP server implementation
│   ├── server_main.cpp       # Server entry point
│   └── client.cpp            # TCP client
├── tests/
│   └── test_orderbook.cpp    # Google Test unit tests
├── docs/
│   └── demo.png              # Demo screenshot
└── .github/workflows/
    └── ci.yml                # GitHub Actions CI
```

## Technical Details

- **Order book**: `std::map<double, std::deque<Order>>` — sorted by price, FIFO within each price level
- **Bid side**: `std::greater<double>` comparator so highest bid is always at `begin()`
- **Ask side**: default `std::less<double>` so lowest ask is always at `begin()`
- **Matching**: aggressive order walks the opposite side of the book until fully filled or no more matching prices
- **Concurrency**: `ThreadSafeQueue` uses `std::mutex` + `std::condition_variable` for producer-consumer synchronization
- **Networking**: POSIX TCP sockets, one thread per client, `std::mutex` protects shared order book state
- **Benchmarking**: `std::chrono::steady_clock` for per-order latency measurement

## Roadmap

- [x] Core matching engine with price-time priority
- [x] Multi-symbol support
- [x] Random order simulator with benchmarking
- [x] Multithreading with thread-safe queue
- [x] TCP server and client
- [ ] Market orders
- [ ] Historical replay from CSV
- [ ] Trade log export