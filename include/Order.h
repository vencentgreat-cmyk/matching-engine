#pragma once
#include <string>
#include <chrono>

enum class Side {
    BUY,
    SELL
};

struct Order {
    int id;
    Side side;
    std::string symbol;
    int quantity;
    double price;
    std::chrono::steady_clock::time_point timestamp;
};