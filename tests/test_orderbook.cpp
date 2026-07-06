#include <gtest/gtest.h>
#include "OrderBook.h"

int nextTestId = 1000;

Order makeOrder(Side side, const std::string& symbol, int qty, double price) {
    return Order{nextTestId++, side, symbol, qty, price,
                 std::chrono::steady_clock::now()};
}

// 买单挂上去，没有卖方，不应该成交
TEST(OrderBook, BuyOrderNoMatch) {
    OrderBook book;
    Order order = makeOrder(Side::BUY, "AMD", 100, 145.20);
    auto trades = book.addOrder(order);
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(order.quantity, 100);
}

// 卖单挂上去，没有买方，不应该成交
TEST(OrderBook, SellOrderNoMatch) {
    OrderBook book;
    Order order = makeOrder(Side::SELL, "AMD", 50, 150.00);
    auto trades = book.addOrder(order);
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(order.quantity, 50);
}

// 卖单价格高于买单，不应该成交
TEST(OrderBook, NoMatchWhenPricesDontCross) {
    OrderBook book;
    Order buy = makeOrder(Side::BUY, "AMD", 100, 145.00);
    Order sell = makeOrder(Side::SELL, "AMD", 50, 146.00);
    book.addOrder(buy);
    auto trades = book.addOrder(sell);
    EXPECT_TRUE(trades.empty());
}

// 完全成交：卖单数量 == 买单数量
TEST(OrderBook, FullMatch) {
    OrderBook book;
    Order buy = makeOrder(Side::BUY, "AMD", 100, 145.20);
    book.addOrder(buy);

    Order sell = makeOrder(Side::SELL, "AMD", 100, 145.00);
    auto trades = book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 100);
    EXPECT_DOUBLE_EQ(trades[0].price, 145.20);
    EXPECT_EQ(sell.quantity, 0);
}

// 部分成交：卖单数量 < 买单数量
TEST(OrderBook, PartialFill) {
    OrderBook book;
    Order buy = makeOrder(Side::BUY, "AMD", 100, 145.20);
    book.addOrder(buy);

    Order sell = makeOrder(Side::SELL, "AMD", 60, 145.00);
    auto trades = book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 60);
    EXPECT_EQ(sell.quantity, 0);
    // 买方应该还剩 40
}

// 价格优先：高出价先成交
TEST(OrderBook, PricePriority) {
    OrderBook book;
    Order buy1 = makeOrder(Side::BUY, "AMD", 100, 145.00);
    Order buy2 = makeOrder(Side::BUY, "AMD", 100, 146.00);
    book.addOrder(buy1);
    book.addOrder(buy2);

    Order sell = makeOrder(Side::SELL, "AMD", 50, 144.00);
    auto trades = book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1);
    // 应该和出价更高的 buy2 成交
    EXPECT_EQ(trades[0].buyOrderId, buy2.id);
    EXPECT_DOUBLE_EQ(trades[0].price, 146.00);
}

// 时间优先：同价格先到的先成交
TEST(OrderBook, TimePriority) {
    OrderBook book;
    Order buy1 = makeOrder(Side::BUY, "AMD", 100, 145.00);
    Order buy2 = makeOrder(Side::BUY, "AMD", 100, 145.00);
    book.addOrder(buy1);
    book.addOrder(buy2);

    Order sell = makeOrder(Side::SELL, "AMD", 50, 145.00);
    auto trades = book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1);
    // 应该和先到的 buy1 成交
    EXPECT_EQ(trades[0].buyOrderId, buy1.id);
}

// 一笔卖单吃掉多个买单
TEST(OrderBook, MultipleMatchesOneSell) {
    OrderBook book;
    Order buy1 = makeOrder(Side::BUY, "AMD", 50, 146.00);
    Order buy2 = makeOrder(Side::BUY, "AMD", 50, 145.00);
    book.addOrder(buy1);
    book.addOrder(buy2);

    Order sell = makeOrder(Side::SELL, "AMD", 80, 144.00);
    auto trades = book.addOrder(sell);

    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 50);  // 先吃 146.00 的
    EXPECT_EQ(trades[1].quantity, 30);  // 再吃 145.00 的 30 股
    EXPECT_EQ(sell.quantity, 0);
}

// 取消订单
TEST(OrderBook, CancelOrder) {
    OrderBook book;
    Order buy = makeOrder(Side::BUY, "AMD", 100, 145.00);
    book.addOrder(buy);

    EXPECT_TRUE(book.cancelOrder(buy.id));
    // 取消后，卖单不应该和它成交
    Order sell = makeOrder(Side::SELL, "AMD", 50, 144.00);
    auto trades = book.addOrder(sell);
    EXPECT_TRUE(trades.empty());
}

// 取消不存在的订单
TEST(OrderBook, CancelNonExistent) {
    OrderBook book;
    EXPECT_FALSE(book.cancelOrder(99999));
}

// 成交价用挂单方的价格
TEST(OrderBook, TradeAtPassivePrice) {
    OrderBook book;
    Order buy = makeOrder(Side::BUY, "AMD", 100, 146.00);
    book.addOrder(buy);

    Order sell = makeOrder(Side::SELL, "AMD", 100, 144.00);
    auto trades = book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1);
    // 成交价应该是挂单方（买方）的 146.00，不是卖方的 144.00
    EXPECT_DOUBLE_EQ(trades[0].price, 146.00);
}
