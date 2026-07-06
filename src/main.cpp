#include <iostream>
#include <sstream>
#include <map>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include "OrderBook.h"
#include "ThreadSafeQueue.h"

// ---- 共享状态 ----
std::atomic<int> nextId{1};
std::atomic<int> totalTrades{0};

struct MatchResult {
    Order order;
    std::vector<Trade> trades;
};

// ---- 输出函数 ----
void printTrades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "[TRADE] " << t.quantity << " " << t.symbol
                  << " @ " << std::fixed << std::setprecision(2) << t.price
                  << " | Buyer: Order#" << t.buyOrderId
                  << " Seller: Order#" << t.sellOrderId << std::endl;
    }
}

void printHelp() {
    std::cout << "\nCommands:\n"
              << "  BUY <symbol> <qty> <price>    Place a buy order\n"
              << "  SELL <symbol> <qty> <price>    Place a sell order\n"
              << "  CANCEL <order_id>              Cancel an order\n"
              << "  BOOK [symbol]                  Show order book (all or one symbol)\n"
              << "  SIM <symbol> <count>           Simulate random orders (single-threaded)\n"
              << "  MSIM <symbol> <count>          Multi-threaded simulation\n"
              << "  HELP                           Show this message\n"
              << "  QUIT                           Exit\n" << std::endl;
}

// ---- 单线程模拟（保留原版） ----
void runSimulation(std::map<std::string, OrderBook>& books,
                   const std::string& symbol, int count) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> qtyDist(10, 200);
    std::uniform_real_distribution<double> priceDist(140.0, 150.0);

    OrderBook& book = books[symbol];
    int trades = 0;

    auto startAll = std::chrono::steady_clock::now();
    double totalLatencyUs = 0.0;

    for (int i = 0; i < count; ++i) {
        Side side = sideDist(rng) == 0 ? Side::BUY : Side::SELL;
        double price = std::round(priceDist(rng) * 100.0) / 100.0;
        int qty = qtyDist(rng);

        Order order{nextId++, side, symbol, qty, price,
                    std::chrono::steady_clock::now()};

        auto t1 = std::chrono::steady_clock::now();
        auto result = book.addOrder(order);
        auto t2 = std::chrono::steady_clock::now();

        totalLatencyUs += std::chrono::duration<double, std::micro>(t2 - t1).count();
        trades += result.size();
    }

    auto endAll = std::chrono::steady_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(endAll - startAll).count();

    std::cout << "\n=== Single-Thread Simulation ===\n"
              << "  Symbol:          " << symbol << "\n"
              << "  Orders:          " << count << "\n"
              << "  Trades:          " << trades << "\n"
              << "  Total time:      " << std::fixed << std::setprecision(2)
              << totalTimeMs << " ms\n"
              << "  Avg latency:     " << totalLatencyUs / count << " us/order\n"
              << "  Throughput:      " << std::setprecision(0)
              << count / (totalTimeMs / 1000.0) << " orders/sec\n" << std::endl;
}

// ---- 多线程模拟 ----
void matchingThread(ThreadSafeQueue<Order>& queue,
                    OrderBook& book,
                    std::atomic<int>& tradeCount,
                    double& totalLatencyUs,
                    int& processedCount) {
    totalLatencyUs = 0.0;
    processedCount = 0;

    while (true) {
        auto maybeOrder = queue.pop();
        if (!maybeOrder.has_value()) break;  // 队列已停止且为空

        Order order = std::move(maybeOrder.value());

        auto t1 = std::chrono::steady_clock::now();
        auto trades = book.addOrder(order);
        auto t2 = std::chrono::steady_clock::now();

        totalLatencyUs += std::chrono::duration<double, std::micro>(t2 - t1).count();
        tradeCount += trades.size();
        processedCount++;
    }
}

void runMultiThreadSim(const std::string& symbol, int count) {
    ThreadSafeQueue<Order> queue;
    OrderBook book;
    std::atomic<int> tradeCount{0};
    double matchLatencyUs = 0.0;
    int processedCount = 0;

    auto startAll = std::chrono::steady_clock::now();

    // 启动撮合线程
    std::thread matcher(matchingThread,
                        std::ref(queue),
                        std::ref(book),
                        std::ref(tradeCount),
                        std::ref(matchLatencyUs),
                        std::ref(processedCount));

    // 当前线程作为 ingestion 线程，生成订单并推入队列
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> qtyDist(10, 200);
    std::uniform_real_distribution<double> priceDist(140.0, 150.0);

    for (int i = 0; i < count; ++i) {
        Side side = sideDist(rng) == 0 ? Side::BUY : Side::SELL;
        double price = std::round(priceDist(rng) * 100.0) / 100.0;
        int qty = qtyDist(rng);

        Order order{nextId++, side, symbol, qty, price,
                    std::chrono::steady_clock::now()};

        queue.push(std::move(order));
    }

    // 通知撮合线程：没有更多订单了
    queue.stop();

    // 等待撮合线程完成
    matcher.join();

    auto endAll = std::chrono::steady_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(endAll - startAll).count();

    std::cout << "\n=== Multi-Thread Simulation ===\n"
              << "  Symbol:          " << symbol << "\n"
              << "  Orders:          " << count << "\n"
              << "  Trades:          " << tradeCount.load() << "\n"
              << "  Total time:      " << std::fixed << std::setprecision(2)
              << totalTimeMs << " ms\n"
              << "  Avg match latency: " << matchLatencyUs / processedCount << " us/order\n"
              << "  Throughput:      " << std::setprecision(0)
              << count / (totalTimeMs / 1000.0) << " orders/sec\n"
              << "  (Ingestion and matching ran on separate threads)\n" << std::endl;
}

// ---- 主循环 ----
int main() {
    std::map<std::string, OrderBook> books;
    std::string line;

    std::cout << "=== Matching Engine ===" << std::endl;
    printHelp();

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        for (auto& c : command) c = toupper(c);

        if (command == "BUY" || command == "SELL") {
            std::string symbol;
            int qty;
            double price;

            if (!(iss >> symbol >> qty >> price)) {
                std::cout << "Usage: " << command << " <symbol> <qty> <price>" << std::endl;
                continue;
            }

            for (auto& c : symbol) c = toupper(c);

            Side side = (command == "BUY") ? Side::BUY : Side::SELL;
            Order order{nextId++, side, symbol, qty, price,
                        std::chrono::steady_clock::now()};

            std::cout << "[ORDER] id=" << order.id << " "
                      << command << " " << qty << " " << symbol
                      << " @ " << std::fixed << std::setprecision(2) << price
                      << std::endl;

            auto trades = books[symbol].addOrder(order);
            printTrades(trades);

            if (order.quantity > 0 && !trades.empty()) {
                std::cout << "[PARTIAL] " << order.quantity << " remaining on book" << std::endl;
            }

        } else if (command == "CANCEL") {
            int orderId;
            if (!(iss >> orderId)) {
                std::cout << "Usage: CANCEL <order_id>" << std::endl;
                continue;
            }
            bool found = false;
            for (auto& [sym, book] : books) {
                if (book.cancelOrder(orderId)) {
                    std::cout << "[CANCELLED] Order#" << orderId
                              << " (" << sym << ")" << std::endl;
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cout << "[NOT FOUND] Order#" << orderId << std::endl;
            }

        } else if (command == "BOOK") {
            std::string symbol;
            if (iss >> symbol) {
                for (auto& c : symbol) c = toupper(c);
                if (books.count(symbol)) {
                    std::cout << "\n--- " << symbol << " ---";
                    books[symbol].print();
                } else {
                    std::cout << "No orders for " << symbol << std::endl;
                }
            } else {
                if (books.empty()) {
                    std::cout << "Order book is empty." << std::endl;
                } else {
                    for (auto& [sym, book] : books) {
                        std::cout << "\n--- " << sym << " ---";
                        book.print();
                    }
                }
            }

        } else if (command == "SIM") {
            std::string symbol;
            int count;
            if (!(iss >> symbol >> count)) {
                std::cout << "Usage: SIM <symbol> <count>" << std::endl;
                continue;
            }
            for (auto& c : symbol) c = toupper(c);
            if (count <= 0 || count > 10000000) {
                std::cout << "Count must be between 1 and 10,000,000" << std::endl;
                continue;
            }
            runSimulation(books, symbol, count);

        } else if (command == "MSIM") {
            std::string symbol;
            int count;
            if (!(iss >> symbol >> count)) {
                std::cout << "Usage: MSIM <symbol> <count>" << std::endl;
                continue;
            }
            for (auto& c : symbol) c = toupper(c);
            if (count <= 0 || count > 10000000) {
                std::cout << "Count must be between 1 and 10,000,000" << std::endl;
                continue;
            }
            runMultiThreadSim(symbol, count);

        } else if (command == "HELP") {
            printHelp();

        } else if (command == "QUIT" || command == "EXIT") {
            std::cout << "Shutting down." << std::endl;
            break;

        } else {
            std::cout << "Unknown command. Type HELP for options." << std::endl;
        }
    }

    return 0;
}
