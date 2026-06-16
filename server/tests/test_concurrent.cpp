#include "core/OrderBook.h"
#include "concurrent/HazardPointer.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <vector>
#include <thread>
#include <atomic>

int testConcurrentStress() {
    int failed = 0;
    std::cout << "  Testing concurrent stress with 6 threads..." << std::endl;

    HFT::OrderBook orderBook("STRESS-USDT");
    std::atomic<uint64_t> orderIdCounter(0);
    std::atomic<bool> running(true);
    std::atomic<uint64_t> totalOps(0);
    std::atomic<uint64_t> totalCancels(0);
    std::atomic<uint64_t> totalTrades(0);

    orderBook.setTradeCallback([&](const HFT::Trade& trade) {
        totalTrades.fetch_add(1, std::memory_order_relaxed);
    });

    const int NUM_THREADS = 6;
    const int ORDERS_PER_THREAD = 50000;

    auto worker = [&](int threadId) {
        std::mt19937 gen(threadId * 12345 + 67890);
        std::uniform_int_distribution<int> sideDist(0, 1);
        std::uniform_int_distribution<uint64_t> qtyDist(1, 20);
        std::normal_distribution<> priceDist(1000000, 500);

        for (int i = 0; i < ORDERS_PER_THREAD; ++i) {
            HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
            int64_t price = static_cast<int64_t>(priceDist(gen));
            price = (price / 50) * 50;
            uint64_t qty = qtyDist(gen);

            HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, qty, "STRESS-USDT");
            orderBook.addOrder(order);
            totalOps.fetch_add(1, std::memory_order_relaxed);

            if (gen() % 5 == 0) {
                uint64_t targetId = (gen() % orderIdCounter.load()) + 1;
                if (orderBook.cancelOrder(targetId)) {
                    totalCancels.fetch_add(1, std::memory_order_relaxed);
                }
            }

            if (i % 100 == 0) {
                orderBook.tryReclaim();
            }
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    orderBook.tryReclaim();

    std::cout << "    Threads: " << NUM_THREADS << std::endl;
    std::cout << "    Total operations: " << totalOps.load() << std::endl;
    std::cout << "    Total cancels: " << totalCancels.load() << std::endl;
    std::cout << "    Total trades: " << totalTrades.load() << std::endl;
    std::cout << "    Total time: " << duration.count() << " ms" << std::endl;
    double opsPerSec = static_cast<double>(totalOps.load()) / (static_cast<double>(duration.count()) / 1000.0);
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << opsPerSec << " ops/sec" << std::endl;
    std::cout << "    Retired pending: " << HFT::HazardPointerManager::instance().retiredCount() << std::endl;

    std::cout << "  ✓ Concurrent stress test completed without crash" << std::endl;

    orderBook.clear();
    return failed;
}

int testConcurrentCancelWhileMatching() {
    int failed = 0;
    std::cout << "  Testing concurrent cancel-while-matching scenario..." << std::endl;

    HFT::OrderBook orderBook("CANCEL-MATCH-USDT");
    std::atomic<uint64_t> orderIdCounter(0);
    std::atomic<bool> running(true);
    std::atomic<uint64_t> crashFlag(0);

    orderBook.setTradeCallback([&](const HFT::Trade& trade) {
    });

    for (int i = 0; i < 500; ++i) {
        int64_t price = 1000000 + ((i % 20) - 10) * 100;
        HFT::Side side = (i % 2 == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, 10, "CANCEL-MATCH-USDT");
        orderBook.addOrder(order);
    }

    auto matchingThread = [&]() {
        std::mt19937 gen(42);
        std::uniform_int_distribution<int> sideDist(0, 1);
        std::normal_distribution<> priceDist(1000000, 300);

        for (int i = 0; i < 100000 && running.load(); ++i) {
            HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
            int64_t price = static_cast<int64_t>(priceDist(gen));
            price = (price / 100) * 100;
            HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, 5, "CANCEL-MATCH-USDT");
            orderBook.addOrder(order);
        }
    };

    auto cancelThread = [&]() {
        std::mt19937 gen(123);
        for (int i = 0; i < 50000 && running.load(); ++i) {
            uint64_t targetId = (gen() % orderIdCounter.load()) + 1;
            orderBook.cancelOrder(targetId);
            if (i % 50 == 0) {
                orderBook.tryReclaim();
            }
        }
    };

    auto snapshotThread = [&]() {
        for (int i = 0; i < 10000 && running.load(); ++i) {
            auto snapshot = orderBook.getSnapshot(10);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(matchingThread);
    std::thread t2(cancelThread);
    std::thread t3(snapshotThread);

    t1.join();
    running.store(false);
    t2.join();
    t3.join();

    orderBook.tryReclaim();

    std::cout << "    Orders processed: " << orderIdCounter.load() << std::endl;
    std::cout << "    Remaining in book: " << orderBook.getTotalOrderCount() << std::endl;
    std::cout << "  ✓ Cancel-while-matching test completed without crash" << std::endl;

    orderBook.clear();
    return failed;
}

int testHazardPointerReclamation() {
    int failed = 0;
    std::cout << "  Testing Hazard Pointer memory reclamation..." << std::endl;

    HFT::OrderBook orderBook("HAZARD-USDT");
    std::atomic<uint64_t> orderIdCounter(0);

    for (int round = 0; round < 10; ++round) {
        std::vector<uint64_t> orderIds;
        for (int i = 0; i < 1000; ++i) {
            HFT::Side side = (i % 2 == 0) ? HFT::Side::BUY : HFT::Side::SELL;
            int64_t price = 1000000 + (i % 50) * 100;
            HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, 10, "HAZARD-USDT");
            orderBook.addOrder(order);
            orderIds.push_back(orderIdCounter.load());
        }

        for (int i = 0; i < 500 && i < static_cast<int>(orderIds.size()); ++i) {
            orderBook.cancelOrder(orderIds[i]);
        }

        orderBook.tryReclaim();
    }

    size_t retiredCount = HFT::HazardPointerManager::instance().retiredCount();
    std::cout << "    Retired nodes pending: " << retiredCount << std::endl;

    orderBook.tryReclaim();
    size_t afterReclaim = HFT::HazardPointerManager::instance().retiredCount();
    std::cout << "    After reclaim: " << afterReclaim << std::endl;

    std::cout << "  ✓ Hazard Pointer reclamation test completed" << std::endl;

    orderBook.clear();
    return failed;
}

int testPerformanceWithConcurrency() {
    int failed = 0;
    std::cout << "  Testing single-thread insert performance..." << std::endl;
    HFT::OrderBook orderBook("PERF-USDT");
    std::atomic<uint64_t> orderIdCounter(0);

    const int NUM_ORDERS = 100000;
    std::vector<HFT::Order*> orders;
    orders.reserve(NUM_ORDERS);

    std::mt19937 gen(42);
    std::uniform_int_distribution<int64_t> priceDist(999000, 1001000);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    for (int i = 0; i < NUM_ORDERS; ++i) {
        HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = priceDist(gen);
        uint64_t qty = qtyDist(gen);
        orders.push_back(new HFT::Order(++orderIdCounter, side, price, qty, "PERF-USDT"));
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (auto order : orders) {
        orderBook.addOrder(order);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgLatency = static_cast<double>(duration.count()) / NUM_ORDERS;
    double ordersPerSec = static_cast<double>(NUM_ORDERS) /
                         (static_cast<double>(duration.count()) / 1000000.0);

    std::cout << "    Total: " << duration.count() << " us" << std::endl;
    std::cout << "    Avg: " << std::fixed << std::setprecision(3) << avgLatency << " us/order" << std::endl;
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << ordersPerSec << " orders/sec" << std::endl;

    assert(avgLatency < 15.0);
    std::cout << "  ✓ Insert latency < 15us average (with hazard pointer overhead)" << std::endl;

    auto snapshotStart = std::chrono::high_resolution_clock::now();
    auto snapshot = orderBook.getSnapshot(10);
    auto snapshotEnd = std::chrono::high_resolution_clock::now();
    auto snapshotDuration = std::chrono::duration_cast<std::chrono::microseconds>(snapshotEnd - snapshotStart);

    std::cout << "  Snapshot: " << snapshotDuration.count() << " us" << std::endl;
    assert(snapshotDuration.count() < 2000);
    std::cout << "  ✓ Snapshot < 2ms" << std::endl;

    std::cout << "  Clearing orderBook..." << std::endl;
    orderBook.clear();
    std::cout << "  OrderBook cleared." << std::endl;

    std::cout << "  Testing cancel performance..." << std::endl;
    HFT::OrderBook cancelBook("CANCEL-PERF-USDT");
    const int CANCEL_COUNT = 20000;
    std::vector<uint64_t> cancelIds;
    cancelIds.reserve(CANCEL_COUNT);

    for (int i = 0; i < CANCEL_COUNT; ++i) {
        HFT::Side side = (i % 2 == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = 1000000 + (i % 100) * 10;
        HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, 1, "CANCEL-PERF-USDT");
        cancelIds.push_back(order->orderId);
        cancelBook.addOrder(order);
    }

    auto cancelStart = std::chrono::high_resolution_clock::now();
    int cancelled = 0;
    for (int i = 0; i < CANCEL_COUNT / 2; ++i) {
        if (cancelBook.cancelOrder(cancelIds[i])) {
            cancelled++;
        }
    }
    cancelBook.tryReclaim();
    auto cancelEnd = std::chrono::high_resolution_clock::now();
    auto cancelDuration = std::chrono::duration_cast<std::chrono::microseconds>(cancelEnd - cancelStart);
    double avgCancel = static_cast<double>(cancelDuration.count()) / std::max(1, cancelled);

    std::cout << "    Cancelled: " << cancelled << " orders" << std::endl;
    std::cout << "    Avg: " << std::fixed << std::setprecision(3) << avgCancel << " us/cancel" << std::endl;
    assert(avgCancel < 10.0);
    std::cout << "  ✓ Cancel latency < 10us average (with hazard pointer overhead)" << std::endl;

    std::cout << "  Clearing cancelBook..." << std::endl;
    cancelBook.clear();
    std::cout << "  CancelBook cleared." << std::endl;

    return failed;
}

int testConcurrent() {
    int failed = 0;

    std::cout << "[TEST] Running Concurrent stress tests..." << std::endl;

    std::cout << "  Step 1: Hazard Pointer reclamation test..." << std::endl;
    failed += testHazardPointerReclamation();
    std::cout << "  Step 1 completed." << std::endl;

    std::cout << "  Step 2: Concurrent stress test..." << std::endl;
    failed += testConcurrentStress();
    std::cout << "  Step 2 completed." << std::endl;

    std::cout << "  Step 3: Cancel-while-matching test..." << std::endl;
    failed += testConcurrentCancelWhileMatching();
    std::cout << "  Step 3 completed." << std::endl;

    std::cout << "  Step 4: Performance with concurrency..." << std::endl;
    failed += testPerformanceWithConcurrency();
    std::cout << "  Step 4 completed." << std::endl;

    return failed;
}
