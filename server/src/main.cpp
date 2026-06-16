#include "core/OrderBook.h"
#include "net/TCPServer.h"
#include "net/WebSocketServer.h"
#include <iostream>
#include <memory>
#include <signal.h>
#include <atomic>
#include <iomanip>
#include <cstdlib>
#include <thread>
#include <random>
#include <chrono>
#include <vector>

std::atomic<bool> g_running(true);

void signalHandler(int) {
    g_running.store(false);
    std::cout << "\n[Main] Shutting down..." << std::endl;
}

void orderSimulator(HFT::OrderBook& orderBook, std::atomic<uint64_t>& orderIdCounter) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 50);
    std::normal_distribution<> priceDist(1000000, 500);

    int64_t basePrice = 1000000;

    std::cout << "[SIM] Starting order simulator..." << std::endl;

    for (int i = 0; i < 10; ++i) {
        int64_t price = basePrice - (i + 1) * 100;
        uint64_t qty = qtyDist(gen) + 10;
        HFT::Order* order = new HFT::Order(++orderIdCounter, HFT::Side::BUY, price, qty, "BTC-USDT");
        orderBook.addOrder(order);
    }
    for (int i = 0; i < 10; ++i) {
        int64_t price = basePrice + (i + 1) * 100;
        uint64_t qty = qtyDist(gen) + 10;
        HFT::Order* order = new HFT::Order(++orderIdCounter, HFT::Side::SELL, price, qty, "BTC-USDT");
        orderBook.addOrder(order);
    }

    std::cout << "[SIM] Initial order book seeded" << std::endl;

    while (g_running.load()) {
        HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = static_cast<int64_t>(priceDist(gen));
        price = (price / 100) * 100;
        uint64_t qty = qtyDist(gen);

        HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, qty, "BTC-USDT");
        orderBook.addOrder(order);

        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (gen() % 100)));
    }
}

void cancelSimulator(HFT::OrderBook& orderBook, std::atomic<uint64_t>& orderIdCounter) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> cancelDist(1, 1000);

    std::cout << "[CANCEL-SIM] Starting cancel simulator..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(2));

    while (g_running.load()) {
        uint64_t targetId = (cancelDist(gen) % orderIdCounter.load()) + 1;
        orderBook.cancelOrder(targetId);
        orderBook.tryReclaim();

        std::this_thread::sleep_for(std::chrono::milliseconds(30 + (gen() % 70)));
    }
}

void highFrequencySimulator(HFT::OrderBook& orderBook, std::atomic<uint64_t>& orderIdCounter, int threadId) {
    std::random_device rd;
    std::mt19937 gen(rd() + threadId);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 20);
    std::normal_distribution<> priceDist(1000000, 300);

    while (g_running.load()) {
        HFT::Side side = (sideDist(gen) == 0) ? HFT::Side::BUY : HFT::Side::SELL;
        int64_t price = static_cast<int64_t>(priceDist(gen));
        price = (price / 50) * 50;
        uint64_t qty = qtyDist(gen);

        HFT::Order* order = new HFT::Order(++orderIdCounter, side, price, qty, "BTC-USDT");
        orderBook.addOrder(order);

        if (gen() % 10 == 0) {
            uint64_t targetId = (gen() % orderIdCounter.load()) + 1;
            orderBook.cancelOrder(targetId);
        }

        orderBook.tryReclaim();
    }
}

int main(int argc, char* argv[]) {
    uint16_t tcpPort = (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 12345;
    uint16_t wsPort = (argc > 2) ? static_cast<uint16_t>(std::atoi(argv[2])) : 8080;

    signal(SIGINT, signalHandler);
#ifdef _WIN32
    signal(SIGBREAK, signalHandler);
#endif

    std::cout << "========================================" << std::endl;
    std::cout << "  HFT Matching Engine v2.0" << std::endl;
    std::cout << "  (Lock-free + Hazard Pointers)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Symbol: BTC-USDT" << std::endl;
    std::cout << "TCP Port (FIX): " << tcpPort << std::endl;
    std::cout << "WS Port (Monitor): " << wsPort << std::endl;
    std::cout << "========================================" << std::endl;

    auto orderBook = std::make_unique<HFT::OrderBook>("BTC-USDT");
    std::atomic<uint64_t> orderIdCounter(0);

    HFT::TCPServer tcpServer(tcpPort, *orderBook);
    HFT::WebSocketServer wsServer(wsPort, *orderBook);

    wsServer.setBroadcastIntervalMs(50);

    orderBook->setTradeCallback([&](const HFT::Trade& trade) {
        double price = trade.price / 10000.0;
        std::cout << "[TRADE] ID=" << trade.tradeId
                  << " Price=" << std::fixed << std::setprecision(4) << price
                  << " Qty=" << trade.quantity << std::endl;
        wsServer.broadcastTrade(trade);
    });

    if (!tcpServer.start()) {
        std::cerr << "[ERROR] Failed to start TCP server on port " << tcpPort << std::endl;
        return 1;
    }

    if (!wsServer.start()) {
        std::cerr << "[ERROR] Failed to start WebSocket server on port " << wsPort << std::endl;
        tcpServer.stop();
        return 1;
    }

    std::thread simThread(orderSimulator, std::ref(*orderBook), std::ref(orderIdCounter));
    std::thread cancelThread(cancelSimulator, std::ref(*orderBook), std::ref(orderIdCounter));

    const int HF_THREADS = 3;
    std::vector<std::thread> hfThreads;
    for (int i = 0; i < HF_THREADS; ++i) {
        hfThreads.emplace_back(highFrequencySimulator, std::ref(*orderBook), std::ref(orderIdCounter), i);
    }

    std::cout << "[Main] Servers started successfully" << std::endl;
    std::cout << "[Main] " << (3 + HF_THREADS) << " concurrent sim threads running" << std::endl;
    std::cout << "[Main] Press Ctrl+C to stop..." << std::endl;

    while (g_running.load()) {
        std::cout << "\r[STATS] Orders=" << orderBook->getTotalOrderCount()
                  << " BidLevels=" << orderBook->getBidCount()
                  << " AskLevels=" << orderBook->getAskCount()
                  << " BestBid=" << std::fixed << std::setprecision(4)
                  << (orderBook->getBestBid() / 10000.0)
                  << " BestAsk=" << (orderBook->getBestAsk() / 10000.0)
                  << " TCP=" << tcpServer.getConnectedClients()
                  << " WS=" << wsServer.getConnectedClients()
                  << " Retired=" << HFT::HazardPointerManager::instance().retiredCount()
                  << "          " << std::flush;

        orderBook->tryReclaim();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << std::endl;
    if (simThread.joinable()) simThread.join();
    if (cancelThread.joinable()) cancelThread.join();
    for (auto& t : hfThreads) {
        if (t.joinable()) t.join();
    }
    tcpServer.stop();
    wsServer.stop();
    std::cout << "[Main] Servers stopped" << std::endl;

    return 0;
}
