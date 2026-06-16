#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <atomic>
#include <algorithm>

namespace HFT {

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    ICEBERG = 2
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

    bool isIceberg;
    uint64_t peakQuantity;
    uint64_t visibleQuantity;
    uint64_t hiddenQuantity;
    uint64_t visibleFilledQuantity;

    Order()
        : orderId(0), clientOrderId(0), side(Side::BUY), type(OrderType::LIMIT),
          price(0), quantity(0), filledQuantity(0), timestamp(0),
          status(OrderStatus::PENDING), prev(nullptr), next(nullptr),
          retired(false),
          isIceberg(false), peakQuantity(0), visibleQuantity(0),
          hiddenQuantity(0), visibleFilledQuantity(0) {}

    Order(uint64_t id, Side s, int64_t p, uint64_t q, const std::string& sym,
          OrderType t = OrderType::LIMIT)
        : orderId(id), clientOrderId(id), side(s), type(t), price(p),
          quantity(q), filledQuantity(0), status(OrderStatus::ACTIVE),
          symbol(sym), prev(nullptr), next(nullptr),
          retired(false),
          isIceberg(false), peakQuantity(0), visibleQuantity(0),
          hiddenQuantity(0), visibleFilledQuantity(0) {
        timestamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
    }

    static Order* createIceberg(uint64_t id, Side s, int64_t p,
                                 uint64_t totalQty, uint64_t peak,
                                 const std::string& sym) {
        Order* order = new Order(id, s, p, totalQty, sym, OrderType::ICEBERG);
        order->isIceberg = true;
        order->peakQuantity = peak;
        order->visibleQuantity = std::min(peak, totalQty);
        order->hiddenQuantity = totalQty - order->visibleQuantity;
        order->visibleFilledQuantity = 0;
        return order;
    }

    uint64_t getRemainingQuantity() const {
        if (isIceberg) {
            return visibleQuantity - visibleFilledQuantity;
        }
        return quantity - filledQuantity;
    }

    uint64_t getTotalRemainingQuantity() const {
        if (isIceberg) {
            return (visibleQuantity - visibleFilledQuantity) + hiddenQuantity;
        }
        return quantity - filledQuantity;
    }

    bool isFilled() const {
        if (isIceberg) {
            return visibleFilledQuantity + (quantity - visibleQuantity - hiddenQuantity) >= quantity;
        }
        return filledQuantity >= quantity;
    }

    bool isTotallyFilled() const {
        if (isIceberg) {
            return hiddenQuantity == 0 && visibleFilledQuantity >= visibleQuantity;
        }
        return filledQuantity >= quantity;
    }

    uint64_t getVisibleRemaining() const {
        if (isIceberg) {
            return visibleQuantity - visibleFilledQuantity;
        }
        return quantity - filledQuantity;
    }

    bool isVisiblePeakFilled() const {
        if (!isIceberg) return false;
        return visibleFilledQuantity >= visibleQuantity;
    }

    bool replenishPeak() {
        if (!isIceberg || hiddenQuantity == 0) return false;
        if (!isVisiblePeakFilled()) return false;

        uint64_t nextPeak = std::min(peakQuantity, hiddenQuantity);
        filledQuantity += visibleFilledQuantity;
        visibleFilledQuantity = 0;
        visibleQuantity = nextPeak;
        hiddenQuantity -= nextPeak;

        return true;
    }

    void fillVisible(uint64_t qty) {
        if (isIceberg) {
            visibleFilledQuantity += qty;
        } else {
            filledQuantity += qty;
        }
    }

    uint64_t getTotalFilled() const {
        if (isIceberg) {
            return filledQuantity + visibleFilledQuantity;
        }
        return filledQuantity;
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
    bool buyIsIceberg;
    bool sellIsIceberg;

    Trade()
        : tradeId(0), buyOrderId(0), sellOrderId(0), price(0),
          quantity(0), timestamp(0),
          buyIsIceberg(false), sellIsIceberg(false) {}

    Trade(uint64_t tid, uint64_t bid, uint64_t sid, int64_t p, uint64_t q,
          const std::string& sym)
        : tradeId(tid), buyOrderId(bid), sellOrderId(sid), price(p),
          quantity(q), symbol(sym),
          buyIsIceberg(false), sellIsIceberg(false) {
        timestamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
    }
};

}
