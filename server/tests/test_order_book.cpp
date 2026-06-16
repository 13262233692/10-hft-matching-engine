#include "core/OrderBook.h"
#include <cassert>
#include <iostream>

int testOrderBook() {
    int failed = 0;
    HFT::OrderBook orderBook("BTC-USDT");

    HFT::Order* buy1 = new HFT::Order(1, HFT::Side::BUY, 1000000, 10, "BTC-USDT");
    HFT::Order* buy2 = new HFT::Order(2, HFT::Side::BUY, 999900, 5, "BTC-USDT");
    HFT::Order* sell1 = new HFT::Order(3, HFT::Side::SELL, 1000100, 8, "BTC-USDT");
    HFT::Order* sell2 = new HFT::Order(4, HFT::Side::SELL, 1000200, 12, "BTC-USDT");

    orderBook.addOrder(buy1);
    orderBook.addOrder(buy2);
    orderBook.addOrder(sell1);
    orderBook.addOrder(sell2);

    assert(orderBook.getBestBid() == 1000000);
    assert(orderBook.getBestAsk() == 1000100);
    assert(orderBook.getTotalOrderCount() == 4);
    std::cout << "  ✓ Basic order placement" << std::endl;

    HFT::Order* marketSell = new HFT::Order(5, HFT::Side::SELL, 0, 15, "BTC-USDT", HFT::OrderType::MARKET);
    orderBook.addOrder(marketSell);

    assert(orderBook.getBestBid() == 999900);
    std::cout << "  ✓ Market order matching" << std::endl;

    HFT::Order* limitBuy = new HFT::Order(6, HFT::Side::BUY, 1000150, 20, "BTC-USDT");
    orderBook.addOrder(limitBuy);

    assert(orderBook.getBestAsk() == 1000200);
    std::cout << "  ✓ Limit order matching" << std::endl;

    HFT::Order* cancelTest = new HFT::Order(7, HFT::Side::BUY, 999800, 3, "BTC-USDT");
    orderBook.addOrder(cancelTest);
    assert(orderBook.findOrder(7) != nullptr);

    bool cancelled = orderBook.cancelOrder(7);
    assert(cancelled);
    assert(orderBook.findOrder(7) == nullptr);
    std::cout << "  ✓ Order cancellation" << std::endl;

    auto snapshot = orderBook.getSnapshot(5);
    assert(snapshot.bids.size() > 0);
    assert(snapshot.asks.size() > 0);
    std::cout << "  ✓ Snapshot generation" << std::endl;

    HFT::Order* timeOrder1 = new HFT::Order(10, HFT::Side::BUY, 1000000, 5, "BTC-USDT");
    HFT::Order* timeOrder2 = new HFT::Order(11, HFT::Side::BUY, 1000000, 3, "BTC-USDT");
    orderBook.addOrder(timeOrder1);
    orderBook.addOrder(timeOrder2);

    HFT::Order* sellMatch = new HFT::Order(12, HFT::Side::SELL, 999900, 7, "BTC-USDT");
    orderBook.addOrder(sellMatch);

    assert(orderBook.findOrder(10) == nullptr);
    assert(orderBook.findOrder(11) != nullptr);
    std::cout << "  ✓ Time priority matching" << std::endl;

    orderBook.clear();
    assert(orderBook.getTotalOrderCount() == 0);
    std::cout << "  ✓ Order book clear" << std::endl;

    return failed;
}
