#pragma once
#include "Order.h"
#include <map>
#include <deque>
#include <vector>
#include <iostream>
#include <iomanip>

struct Trade {
    int buyOrderId;
    int sellOrderId;
    std::string symbol;
    int quantity;
    double price;
};

class OrderBook {
public:
    // 注意：引用传递，撮合后 order.quantity 会反映剩余数量
    std::vector<Trade> addOrder(Order& order);
    bool cancelOrder(int orderId);
    void print() const;

private:
    std::map<double, std::deque<Order>, std::greater<double>> bids;
    std::map<double, std::deque<Order>> asks;
    std::vector<Trade> match(Order& order);
};