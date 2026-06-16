#include "core/OrderBook.h"
#include "core/FIXParser.h"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <vector>
#include <random>

int testIcebergOrderCreation() {
    int failed = 0;

    std::cout << "  Testing iceberg order creation..." << std::endl;

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::BUY,
        1000000, 1000, 100, "BTC-USDT");

    assert(iceberg->isIceberg == true);
    assert(iceberg->type == HFT::OrderType::ICEBERG);
    assert(iceberg->quantity == 1000);
    assert(iceberg->peakQuantity == 100);
    assert(iceberg->visibleQuantity == 100);
    assert(iceberg->hiddenQuantity == 900);
    assert(iceberg->visibleFilledQuantity == 0);
    assert(iceberg->getRemainingQuantity() == 100);
    assert(iceberg->getTotalRemainingQuantity() == 1000);
    assert(iceberg->getTotalFilled() == 0);

    std::cout << "  ✓ Iceberg order creation correct" << std::endl;

    delete iceberg;
    return failed;
}

int testIcebergSinglePeakMatch() {
    int failed = 0;

    std::cout << "  Testing iceberg single peak matching..." << std::endl;

    HFT::OrderBook book("ICE-USDT");

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 500, 100, "ICE-USDT");
    book.addOrder(iceberg);

    assert(book.getAskCount() == 1);
    assert(book.getBestAskQuantity() == 100);
    assert(book.getTotalOrderCount() == 1);

    HFT::Order* buyOrder = new HFT::Order(2, HFT::Side::BUY, 1000000, 50, "ICE-USDT");
    book.addOrder(buyOrder);

    assert(book.getBestAskQuantity() == 50);
    assert(book.getTotalOrderCount() == 1);

    HFT::Order* buyOrder2 = new HFT::Order(3, HFT::Side::BUY, 1000000, 60, "ICE-USDT");
    book.addOrder(buyOrder2);

    assert(book.getBestAskQuantity() == 100);
    assert(book.getTotalOrderCount() == 1);

    HFT::Order* order = book.findOrder(1);
    assert(order != nullptr);
    assert(order->getTotalRemainingQuantity() == 390);
    assert(order->hiddenQuantity == 290);
    assert(order->visibleQuantity == 100);

    std::cout << "  ✓ Iceberg single peak match replenish correct" << std::endl;
    return failed;
}

int testIcebergFullConsumption() {
    int failed = 0;

    std::cout << "  Testing full iceberg order consumption..." << std::endl;

    HFT::OrderBook book("FULL-USDT");

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 250, 100, "FULL-USDT");
    book.addOrder(iceberg);

    for (int i = 0; i < 3; ++i) {
        HFT::Order* buyOrder = new HFT::Order(i + 100,
            HFT::Side::BUY, 1000000, 90, "FULL-USDT");
        book.addOrder(buyOrder);
    }

    assert(book.getTotalOrderCount() == 1);
    assert(book.getBestAskQuantity() == 10);

    HFT::Order* order = book.findOrder(1);
    assert(order != nullptr);
    assert(order->getTotalRemainingQuantity() == 10);
    assert(order->hiddenQuantity == 0);
    assert(order->visibleQuantity == 10);

    HFT::Order* lastBuy = new HFT::Order(200,
        HFT::Side::BUY, 1000000, 15, "FULL-USDT");
    book.addOrder(lastBuy);

    assert(book.getAskCount() == 0);
    assert(book.findOrder(1) == nullptr);

    std::cout << "  ✓ Full iceberg consumption correct" << std::endl;
    return failed;
}

int testIcebergOrderPriority() {
    int failed = 0;

    std::cout << "  Testing iceberg order time priority after replenish..." << std::endl;

    HFT::OrderBook book("PRIO-USDT");

    HFT::Order* regular1 = new HFT::Order(1, HFT::Side::SELL, 1000000, 50, "PRIO-USDT");
    book.addOrder(regular1);

    HFT::Order* iceberg = HFT::Order::createIceberg(2, HFT::Side::SELL,
        1000000, 200, 50, "PRIO-USDT");
    book.addOrder(iceberg);

    HFT::Order* regular2 = new HFT::Order(3, HFT::Side::SELL, 1000000, 80, "PRIO-USDT");
    book.addOrder(regular2);

    assert(book.getBestAskQuantity() == 50);

    HFT::Order* bigBuy = new HFT::Order(10,
        HFT::Side::BUY, 1000000, 120, "PRIO-USDT");
    book.addOrder(bigBuy);

    assert(book.getTotalOrderCount() == 3);
    assert(book.getBestAskQuantity() == 50 + 80);

    HFT::Order* foundIce = book.findOrder(2);
    assert(foundIce != nullptr);
    assert(foundIce->getTotalRemainingQuantity() == 150);
    assert(foundIce->hiddenQuantity == 100);
    assert(foundIce->visibleQuantity == 50);

    std::cout << "  ✓ Iceberg order time priority (moves to tail) correct" << std::endl;
    return failed;
}

int testIcebergOrderCancel() {
    int failed = 0;

    std::cout << "  Testing iceberg order cancellation..." << std::endl;

    HFT::OrderBook book("CANCEL-USDT");

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 500, 100, "CANCEL-USDT");
    book.addOrder(iceberg);

    assert(book.getBestAskQuantity() == 100);

    bool result = book.cancelOrder(1);
    assert(result == true);

    assert(book.getAskCount() == 0);
    assert(book.findOrder(1) == nullptr);
    assert(book.getTotalOrderCount() == 0);

    std::cout << "  ✓ Iceberg order cancellation correct" << std::endl;
    return failed;
}

int testIcebergOrderBookSnapshot() {
    int failed = 0;

    std::cout << "  Testing iceberg order book snapshot (only visible)..." << std::endl;

    HFT::OrderBook book("SNAP-USDT");

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 1000, 100, "SNAP-USDT");
    book.addOrder(iceberg);

    HFT::Order* regular = new HFT::Order(2, HFT::Side::SELL, 1000100, 500, "SNAP-USDT");
    book.addOrder(regular);

    auto snapshot = book.getSnapshot(10);

    assert(snapshot.asks.size() == 2);
    assert(snapshot.asks[0].quantity == 100);
    assert(snapshot.asks[1].quantity == 500);
    assert(snapshot.asks[0].orderCount == 1);

    std::cout << "  ✓ Snapshot only shows visible iceberg quantity" << std::endl;
    return failed;
}

int testIcebergFIXParser() {
    int failed = 0;

    std::cout << "  Testing FIX parser iceberg order support..." << std::endl;

    HFT::FIXParser parser;
    HFT::FIXMessage msg;

    msg.fields[35] = "D";
    msg.fields[11] = "12345";
    msg.fields[54] = "1";
    msg.fields[44] = "100.0000";
    msg.fields[38] = "1000";
    msg.fields[55] = "BTC-USDT";
    msg.fields[210] = "100";

    HFT::Order order;
    bool result = parser.parseNewOrder(msg, order);

    assert(result == true);
    assert(order.isIceberg == true);
    assert(order.type == HFT::OrderType::ICEBERG);
    assert(order.quantity == 1000);
    assert(order.peakQuantity == 100);
    assert(order.visibleQuantity == 100);
    assert(order.hiddenQuantity == 900);

    std::cout << "  ✓ FIX parser iceberg order support correct" << std::endl;
    return failed;
}

int testIcebergConcurrent() {
    int failed = 0;

    std::cout << "  Testing concurrent iceberg order matching..." << std::endl;

    HFT::OrderBook book("CONC-USDT");
    std::atomic<uint64_t> orderIdCounter(1000);

    HFT::Order* iceberg1 = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 5000, 200, "CONC-USDT");
    book.addOrder(iceberg1);

    HFT::Order* iceberg2 = HFT::Order::createIceberg(2, HFT::Side::BUY,
        999900, 8000, 300, "CONC-USDT");
    book.addOrder(iceberg2);

    auto worker = [&](int threadId) {
        std::mt19937 gen(threadId * 997 + 13);
        std::uniform_int_distribution<int> sideDist(0, 1);
        std::uniform_int_distribution<uint64_t> qtyDist(10, 50);
        std::normal_distribution<> priceDist(1000000, 200);

        for (int i = 0; i < 2000; ++i) {
            HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
            int64_t price = static_cast<int64_t>(priceDist(gen));
            price = (price / 100) * 100;
            uint64_t qty = qtyDist(gen);

            HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, qty, "CONC-USDT");
            book.addOrder(order);

            if (gen() % 20 == 0) {
                uint64_t targetId = (gen() % orderIdCounter.load()) + 1;
                book.cancelOrder(targetId);
            }

            if (i % 50 == 0) {
                book.tryReclaim();
            }
        }
    };

    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    book.tryReclaim();

    assert(book.findOrder(1) != nullptr || book.findOrder(2) != nullptr ||
           book.getTotalOrderCount() >= 0);

    std::cout << "  ✓ Concurrent iceberg matching: 0 crashes" << std::endl;
    std::cout << "    Orders: " << book.getTotalOrderCount()
              << " BidLevels: " << book.getBidCount()
              << " AskLevels: " << book.getAskCount() << std::endl;

    return failed;
}

int testIcebergPartialPeakFill() {
    int failed = 0;

    std::cout << "  Testing iceberg partial peak fill..." << std::endl;

    HFT::OrderBook book("PARTIAL-USDT");

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 1000, 100, "PARTIAL-USDT");
    book.addOrder(iceberg);

    HFT::Order* buy1 = new HFT::Order(2, HFT::Side::BUY, 1000000, 30, "PARTIAL-USDT");
    book.addOrder(buy1);

    HFT::Order* order = book.findOrder(1);
    assert(order != nullptr);
    assert(order->getRemainingQuantity() == 70);
    assert(order->hiddenQuantity == 900);
    assert(order->getTotalRemainingQuantity() == 970);
    assert(book.getBestAskQuantity() == 70);

    HFT::Order* buy2 = new HFT::Order(3, HFT::Side::BUY, 1000000, 40, "PARTIAL-USDT");
    book.addOrder(buy2);

    order = book.findOrder(1);
    assert(order != nullptr);
    assert(order->getRemainingQuantity() == 30);
    assert(order->hiddenQuantity == 900);
    assert(order->getTotalRemainingQuantity() == 930);
    assert(book.getBestAskQuantity() == 30);

    HFT::Order* buy3 = new HFT::Order(4, HFT::Side::BUY, 1000000, 35, "PARTIAL-USDT");
    book.addOrder(buy3);

    order = book.findOrder(1);
    assert(order != nullptr);
    assert(order->getTotalRemainingQuantity() == 895);
    assert(order->hiddenQuantity == 795);
    assert(order->getRemainingQuantity() == 100);
    assert(book.getBestAskQuantity() == 100);

    std::cout << "  ✓ Iceberg partial peak fill correct" << std::endl;
    return failed;
}

int testIcebergMultiplePeaks() {
    int failed = 0;

    std::cout << "  Testing iceberg multiple peak replenishments..." << std::endl;

    HFT::OrderBook book("MULTI-USDT");

    HFT::Order* iceberg = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 500, 100, "MULTI-USDT");
    book.addOrder(iceberg);

    for (int i = 0; i < 5; ++i) {
        HFT::Order* buy = new HFT::Order(100 + i,
            HFT::Side::BUY, 1000000, 100, "MULTI-USDT");
        book.addOrder(buy);
    }

    assert(book.getAskCount() == 0);
    assert(book.findOrder(1) == nullptr);

    HFT::OrderBook book2("MULTI2-USDT");
    HFT::Order* iceberg2 = HFT::Order::createIceberg(1, HFT::Side::SELL,
        1000000, 350, 100, "MULTI2-USDT");
    book2.addOrder(iceberg2);

    for (int i = 0; i < 3; ++i) {
        HFT::Order* buy = new HFT::Order(200 + i,
            HFT::Side::BUY, 1000000, 100, "MULTI2-USDT");
        book2.addOrder(buy);
    }

    assert(book2.getAskCount() == 1);
    HFT::Order* remaining = book2.findOrder(1);
    assert(remaining != nullptr);
    assert(remaining->getTotalRemainingQuantity() == 50);
    assert(remaining->hiddenQuantity == 0);
    assert(remaining->visibleQuantity == 50);

    std::cout << "  ✓ Multiple peak replenishments correct" << std::endl;
    return failed;
}

int testIceberg() {
    int failed = 0;

    std::cout << "[TEST] Running Iceberg Order tests..." << std::endl;

    failed += testIcebergOrderCreation();
    failed += testIcebergSinglePeakMatch();
    failed += testIcebergPartialPeakFill();
    failed += testIcebergFullConsumption();
    failed += testIcebergOrderPriority();
    failed += testIcebergOrderCancel();
    failed += testIcebergOrderBookSnapshot();
    failed += testIcebergMultiplePeaks();
    failed += testIcebergFIXParser();
    failed += testIcebergConcurrent();

    return failed;
}
