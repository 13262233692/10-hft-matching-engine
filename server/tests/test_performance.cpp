#include "core/OrderBook.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <vector>
#include <algorithm>

int testPerformance() {
    int failed = 0;
    HFT::OrderBook orderBook("BTC-USDT");

    const int NUM_ORDERS = 100000;
    std::vector<HFT::Order*> orders;
    orders.reserve(NUM_ORDERS);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> priceDist(999000, 1001000);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    std::cout << "  Generating " << NUM_ORDERS << " orders..." << std::endl;
    for (int i = 0; i < NUM_ORDERS; ++i) {
        HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = priceDist(gen);
        uint64_t qty = qtyDist(gen);
        orders.push_back(new HFT::Order(i + 1, side, price, qty, "BTC-USDT"));
    }

    std::cout << "  Testing insert performance..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (auto order : orders) {
        orderBook.addOrder(order);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgLatency = static_cast<double>(duration.count()) / NUM_ORDERS;
    double ordersPerSec = static_cast<double>(NUM_ORDERS) /
                         (static_cast<double>(duration.count()) / 1000000.0);

    std::cout << "    Total: " << duration.count() << " μs" << std::endl;
    std::cout << "    Avg: " << std::fixed << std::setprecision(3) << avgLatency << " μs/order" << std::endl;
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << ordersPerSec << " orders/sec" << std::endl;

    assert(avgLatency < 10.0);
    std::cout << "  ✓ Insert latency < 10μs average" << std::endl;

    auto snapshotStart = std::chrono::high_resolution_clock::now();
    auto snapshot = orderBook.getSnapshot(10);
    auto snapshotEnd = std::chrono::high_resolution_clock::now();
    auto snapshotDuration = std::chrono::duration_cast<std::chrono::microseconds>(snapshotEnd - snapshotStart);

    std::cout << "  Snapshot: " << snapshotDuration.count() << " μs" << std::endl;
    assert(snapshotDuration.count() < 1000);
    std::cout << "  ✓ Snapshot < 1ms" << std::endl;

    std::cout << "  Testing cancel performance..." << std::endl;
    HFT::OrderBook cancelBook("CANCEL-TEST");
    const int CANCEL_COUNT = 20000;
    std::vector<uint64_t> orderIds;
    orderIds.reserve(CANCEL_COUNT);

    for (int i = 0; i < CANCEL_COUNT; ++i) {
        HFT::Side side = (i % 2 == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = 1000000 + (i % 100) * 10;
        HFT::Order* order = new HFT::Order(i + 1000000, side, price, 1, "CANCEL-TEST");
        orderIds.push_back(order->orderId);
        cancelBook.addOrder(order);
    }

    auto cancelStart = std::chrono::high_resolution_clock::now();
    int cancelled = 0;
    for (int i = 0; i < CANCEL_COUNT / 2; ++i) {
        if (cancelBook.cancelOrder(orderIds[i])) {
            cancelled++;
        }
    }
    auto cancelEnd = std::chrono::high_resolution_clock::now();
    auto cancelDuration = std::chrono::duration_cast<std::chrono::microseconds>(cancelEnd - cancelStart);
    double avgCancel = static_cast<double>(cancelDuration.count()) / std::max(1, cancelled);

    std::cout << "    Cancelled: " << cancelled << " orders" << std::endl;
    std::cout << "    Avg: " << std::fixed << std::setprecision(3) << avgCancel << " μs/cancel" << std::endl;
    assert(avgCancel < 5.0);
    std::cout << "  ✓ Cancel latency < 5μs average" << std::endl;

    std::cout << "  Testing matching performance..." << std::endl;
    HFT::OrderBook matchBook("ETH-USDT");
    const int MATCH_COUNT = 50000;

    std::vector<HFT::Order*> matchOrders;
    for (int i = 0; i < MATCH_COUNT; ++i) {
        HFT::Side side = (i % 2 == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = 2000000 + ((i % 5) - 2) * 100;
        matchOrders.push_back(new HFT::Order(i + 1, side, price, 1, "ETH-USDT"));
    }

    auto matchStart = std::chrono::high_resolution_clock::now();
    for (auto order : matchOrders) {
        matchBook.addOrder(order);
    }
    auto matchEnd = std::chrono::high_resolution_clock::now();
    auto matchDuration = std::chrono::duration_cast<std::chrono::microseconds>(matchEnd - matchStart);
    double avgMatch = static_cast<double>(matchDuration.count()) / MATCH_COUNT;

    std::cout << "    Avg: " << std::fixed << std::setprecision(3) << avgMatch << " μs/order" << std::endl;
    assert(avgMatch < 1.0);
    std::cout << "  ✓ Matching latency < 1μs average" << std::endl;

    orderBook.clear();
    matchBook.clear();

    return failed;
}
