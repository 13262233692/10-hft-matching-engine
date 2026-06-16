#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <atomic>

namespace HFT {

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1
};

enum class OrderStatus : uint8_t {
    PENDING = 0,
    ACTIVE = 1,
    PARTIAL_FILLED = 2,
    FILLED = 3,
    CANCELLED = 4,
    REJECTED = 5
};

struct Order {
    uint64_t orderId;
    uint64_t clientOrderId;
    Side side;
    OrderType type;
    int64_t price;
    uint64_t quantity;
    uint64_t filledQuantity;
    uint64_t timestamp;
    OrderStatus status;
    std::string symbol;
    Order* prev;
    Order* next;
    std::atomic<bool> retired;

    Order()
        : orderId(0), clientOrderId(0), side(Side::BUY), type(OrderType::LIMIT),
          price(0), quantity(0), filledQuantity(0), timestamp(0),
          status(OrderStatus::PENDING), prev(nullptr), next(nullptr),
          retired(false) {}

    Order(uint64_t id, Side s, int64_t p, uint64_t q, const std::string& sym,
          OrderType t = OrderType::LIMIT)
        : orderId(id), clientOrderId(id), side(s), type(t), price(p),
          quantity(q), filledQuantity(0), status(OrderStatus::ACTIVE),
          symbol(sym), prev(nullptr), next(nullptr),
          retired(false) {
        timestamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
    }

    uint64_t getRemainingQuantity() const {
        return quantity - filledQuantity;
    }

    bool isFilled() const {
        return filledQuantity >= quantity;
    }

    bool isRetired() const {
        return retired.load(std::memory_order_acquire);
    }

    void markRetired() {
        retired.store(true, std::memory_order_release);
    }
};

struct Trade {
    uint64_t tradeId;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    int64_t price;
    uint64_t quantity;
    uint64_t timestamp;
    std::string symbol;

    Trade()
        : tradeId(0), buyOrderId(0), sellOrderId(0), price(0),
          quantity(0), timestamp(0) {}

    Trade(uint64_t tid, uint64_t bid, uint64_t sid, int64_t p, uint64_t q,
          const std::string& sym)
        : tradeId(tid), buyOrderId(bid), sellOrderId(sid), price(p),
          quantity(q), symbol(sym) {
        timestamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
    }
};

}
