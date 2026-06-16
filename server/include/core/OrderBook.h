#pragma once

#include "core/Order.h"
#include "core/RedBlackTree.h"
#include "core/OrderQueue.h"
#include "concurrent/SpinLock.h"
#include "concurrent/HazardPointer.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace HFT {

struct PriceLevel {
    int64_t price;
    uint64_t quantity;
    uint32_t orderCount;
};

struct OrderBookSnapshot {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    std::vector<Trade> trades;
    uint64_t timestamp;
    uint64_t sequence;
};

struct OrderBookDelta {
    enum class Action : uint8_t {
        ADD = 0,
        UPDATE = 1,
        DELETE = 2
    };

    struct PriceLevelDelta {
        Side side;
        Action action;
        int64_t price;
        uint64_t quantity;
        uint32_t orderCount;
    };

    std::vector<PriceLevelDelta> priceLevels;
    std::vector<Trade> trades;
    uint64_t timestamp;
    uint64_t sequence;
};

using TradeCallback = std::function<void(const Trade&)>;
using OrderCallback = std::function<void(const Order&, OrderStatus)>;
using SnapshotCallback = std::function<void(const OrderBookSnapshot&)>;

class OrderBook {
public:
    explicit OrderBook(std::string symbol);
    ~OrderBook();

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    void addOrder(Order* order);

    bool cancelOrder(uint64_t orderId);

    Order* findOrder(uint64_t orderId) const;

    OrderBookSnapshot getSnapshot(size_t depth = 10) const;

    void setTradeCallback(TradeCallback callback) { tradeCallback = std::move(callback); }

    void setOrderCallback(OrderCallback callback) { orderCallback = std::move(callback); }

    void setSnapshotCallback(SnapshotCallback callback) { snapshotCallback = std::move(callback); }

    int64_t getBestBid() const;

    int64_t getBestAsk() const;

    uint64_t getBestBidQuantity() const;

    uint64_t getBestAskQuantity() const;

    size_t getBidCount() const;

    size_t getAskCount() const;

    size_t getTotalOrderCount() const;

    const std::string& getSymbol() const { return symbol; }

    void clear();

    void tryReclaim() {
        HazardPointerManager::instance().scanAndReclaim();
    }

private:
    std::string symbol;
    RedBlackTree bidTree;
    RedBlackTree askTree;
    std::unordered_map<uint64_t, Order*> orderMap;
    std::unordered_map<uint64_t, Side> orderSideMap;
    mutable RWSpinLock bookLock;
    std::mutex callbackMutex;
    uint64_t sequenceNumber;
    uint64_t tradeIdCounter;

    TradeCallback tradeCallback;
    OrderCallback orderCallback;
    SnapshotCallback snapshotCallback;

    void matchOrder(Order* order);

    void matchBuyOrder(Order* order);

    void matchSellOrder(Order* order);

    void addOrderToBook(Order* order);

    void removeOrderFromBook(uint64_t orderId);

    void executeTrade(Order* buyOrder, Order* sellOrder, int64_t price, uint64_t quantity);

    void updatePriceLevel(RedBlackTree& tree, Side side, int64_t price,
                          int64_t quantityDelta, int32_t orderCountDelta);

    OrderQueue* getOrCreateQueue(RedBlackTree& tree, int64_t price);

    OrderQueue* getQueue(RedBlackTree& tree, int64_t price);

    void collectPriceLevels(const RedBlackTree& tree, std::vector<PriceLevel>& levels,
                            size_t depth, bool reverse) const;

    void fireTradeCallback(const Trade& trade);
    void fireOrderCallback(const Order& order, OrderStatus status);
};

}
