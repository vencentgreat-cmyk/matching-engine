#pragma once
#include <string>
#include <chrono>

enum class Side {
    BUY,
    SELL
};

enum class OrderType {
    LIMIT,
    MARKET
};

struct Order {
    int id;
    Side side;
    std::string symbol;
    int quantity;
    double price;
    std::chrono::steady_clock::time_point timestamp;
    OrderType type = OrderType::LIMIT;
};