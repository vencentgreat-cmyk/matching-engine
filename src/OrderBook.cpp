#include "OrderBook.h"

std::vector<Trade> OrderBook::addOrder(Order& order) {
    std::vector<Trade> trades = match(order);
    if (order.quantity > 0) {
        if (order.side == Side::BUY) {
            auto& queue = bids[order.price];
            queue.push_back(order);
            orderIndex[order.id] = {Side::BUY, order.price, std::prev(queue.end())};
        } else {
            auto& queue = asks[order.price];
            queue.push_back(order);
            orderIndex[order.id] = {Side::SELL, order.price, std::prev(queue.end())};
        }
    }
    return trades;
}

std::vector<Trade> OrderBook::match(Order& order) {
    std::vector<Trade> trades;
    if (order.side == Side::BUY) {
        while (order.quantity > 0 && !asks.empty()) {
            auto& [bestAskPrice, bestAskQueue] = *asks.begin();
            if (bestAskPrice > order.price) break;
            Order& sellOrder = bestAskQueue.front();
            int fillQty = std::min(order.quantity, sellOrder.quantity);
            trades.push_back({order.id, sellOrder.id, order.symbol, fillQty, bestAskPrice});
            order.quantity -= fillQty;
            sellOrder.quantity -= fillQty;
            if (sellOrder.quantity == 0) {
                orderIndex.erase(sellOrder.id);          // 关键：同步索引
                bestAskQueue.pop_front();
                if (bestAskQueue.empty()) asks.erase(asks.begin());
            }
        }
    } else {
        while (order.quantity > 0 && !bids.empty()) {
            auto& [bestBidPrice, bestBidQueue] = *bids.begin();
            if (bestBidPrice < order.price) break;
            Order& buyOrder = bestBidQueue.front();
            int fillQty = std::min(order.quantity, buyOrder.quantity);
            trades.push_back({buyOrder.id, order.id, order.symbol, fillQty, bestBidPrice});
            order.quantity -= fillQty;
            buyOrder.quantity -= fillQty;
            if (buyOrder.quantity == 0) {
                orderIndex.erase(buyOrder.id);           // 关键：同步索引
                bestBidQueue.pop_front();
                if (bestBidQueue.empty()) bids.erase(bids.begin());
            }
        }
    }
    return trades;
}

bool OrderBook::cancelOrder(int orderId) {
    auto it = orderIndex.find(orderId);
    if (it == orderIndex.end()) return false;   // O(1) 查找

    const auto& loc = it->second;
    if (loc.side == Side::BUY) {
        auto& queue = bids[loc.price];
        queue.erase(loc.it);                    // O(1) 删除
        if (queue.empty()) bids.erase(loc.price);
    } else {
        auto& queue = asks[loc.price];
        queue.erase(loc.it);
        if (queue.empty()) asks.erase(loc.price);
    }
    orderIndex.erase(it);                       // O(1) 索引清理
    return true;
}

void OrderBook::print() const {
    print(std::cout);
}

void OrderBook::print(std::ostream& out) const {
    out << "\n=== Order Book ===" << std::endl;
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        int totalQty = 0;
        for (const auto& o : it->second) totalQty += o.quantity;
        out << "                     ASK "
            << std::fixed << std::setprecision(2)
            << it->first << " x " << totalQty << std::endl;
    }
    out << "  --------------------" << std::endl;
    for (auto it = bids.begin(); it != bids.end(); ++it) {
        int totalQty = 0;
        for (const auto& o : it->second) totalQty += o.quantity;
        out << "  BID " << std::fixed << std::setprecision(2)
            << it->first << " x " << totalQty << std::endl;
    }
    out << std::endl;
}
