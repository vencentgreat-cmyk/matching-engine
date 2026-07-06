#include "OrderBook.h"

std::vector<Trade> OrderBook::addOrder(Order& order) {
    std::vector<Trade> trades = match(order);
    if (order.quantity > 0) {
        if (order.side == Side::BUY) {
            bids[order.price].push_back(order);
        } else {
            asks[order.price].push_back(order);
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
                bestBidQueue.pop_front();
                if (bestBidQueue.empty()) bids.erase(bids.begin());
            }
        }
    }
    return trades;
}

bool OrderBook::cancelOrder(int orderId) {
    for (auto& [price, queue] : bids) {
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (it->id == orderId) {
                queue.erase(it);
                if (queue.empty()) bids.erase(price);
                return true;
            }
        }
    }
    for (auto& [price, queue] : asks) {
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (it->id == orderId) {
                queue.erase(it);
                if (queue.empty()) asks.erase(price);
                return true;
            }
        }
    }
    return false;
}

void OrderBook::print() const {
    std::cout << "\n=== Order Book ===" << std::endl;

    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        int totalQty = 0;
        for (const auto& o : it->second) totalQty += o.quantity;
        std::cout << "                     ASK "
                  << std::fixed << std::setprecision(2)
                  << it->first << " x " << totalQty << std::endl;
    }

    std::cout << "  --------------------" << std::endl;

    for (auto it = bids.begin(); it != bids.end(); ++it) {
        int totalQty = 0;
        for (const auto& o : it->second) totalQty += o.quantity;
        std::cout << "  BID " << std::fixed << std::setprecision(2)
                  << it->first << " x " << totalQty << std::endl;
    }
    std::cout << std::endl;
}