#pragma once
#include "Order.h"
#include <map>
#include <list>
#include <vector>
#include <unordered_map>
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
    std::vector<Trade> addOrder(Order& order);
    bool cancelOrder(int orderId);
    void print() const;

private:
    std::map<double, std::list<Order>, std::greater<double>> bids;
    std::map<double, std::list<Order>> asks;

    struct OrderLocation {
        Side side;
        double price;
        std::list<Order>::iterator it;
    };
    std::unordered_map<int, OrderLocation> orderIndex;

    std::vector<Trade> match(Order& order);
};
