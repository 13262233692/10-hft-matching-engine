#include "core/OrderBook.h"
#include <algorithm>

namespace HFT {

OrderBook::OrderBook(std::string sym)
    : symbol(std::move(sym)), sequenceNumber(0), tradeIdCounter(0) {}

OrderBook::~OrderBook() {
    clear();
}

void OrderBook::clear() {
    WriteGuard guard(bookLock);
    for (auto& [id, order] : orderMap) {
        if (!order->isRetired()) {
            order->markRetired();
            HazardPointerManager::instance().retireNode(
                order, &SafeDeleter::deleteOrder);
        }
    }
    orderMap.clear();
    orderSideMap.clear();
    bidTree.clear();
    askTree.clear();
    sequenceNumber = 0;
    tradeIdCounter = 0;
}

OrderQueue* OrderBook::getOrCreateQueue(RedBlackTree& tree, int64_t price) {
    RBNode* node = tree.find(price);
    if (!node) {
        node = tree.insert(price);
        node->queue = new OrderQueue();
    }
    return static_cast<OrderQueue*>(node->queue);
}

OrderQueue* OrderBook::getQueue(RedBlackTree& tree, int64_t price) {
    RBNode* node = tree.find(price);
    if (node && node->queue) {
        return static_cast<OrderQueue*>(node->queue);
    }
    return nullptr;
}

void OrderBook::updatePriceLevel(RedBlackTree& tree, Side, int64_t price,
                               int64_t quantityDelta, int32_t orderCountDelta) {
    RBNode* node = tree.find(price);
    if (!node) return;

    node->totalQuantity = static_cast<uint64_t>(
        static_cast<int64_t>(node->totalQuantity) + quantityDelta);
    node->orderCount = static_cast<uint32_t>(
        static_cast<int32_t>(node->orderCount) + orderCountDelta);

    if (node->orderCount == 0) {
        OrderQueue* q = static_cast<OrderQueue*>(node->queue);
        if (q) {
            delete q;
        }
        node->queue = nullptr;
        tree.remove(price);
    }
}

void OrderBook::fireTradeCallback(const Trade& trade) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (tradeCallback) {
        tradeCallback(trade);
    }
}

void OrderBook::fireOrderCallback(const Order& order, OrderStatus status) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (orderCallback) {
        orderCallback(order, status);
    }
}

void OrderBook::addOrder(Order* order) {
    if (!order) return;

    std::vector<std::pair<Order*, OrderStatus>> orderNotifications;
    Order* orderToDelete = nullptr;

    {
        WriteGuard guard(bookLock);

        order->orderId = order->clientOrderId;
        order->timestamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();

        if (orderCallback) {
            orderNotifications.push_back({order, OrderStatus::PENDING});
        }

        matchOrder(order);

        if (!order->isFilled() && order->status != OrderStatus::REJECTED) {
            addOrderToBook(order);
            order->status = OrderStatus::ACTIVE;
            if (orderCallback) {
                orderNotifications.push_back({order, OrderStatus::ACTIVE});
            }
        } else if (order->isFilled()) {
            order->status = OrderStatus::FILLED;
            if (orderCallback) {
                orderNotifications.push_back({order, OrderStatus::FILLED});
            }
            orderToDelete = order;
        } else {
            orderToDelete = order;
        }

        sequenceNumber++;
    }

    for (auto& [ord, st] : orderNotifications) {
        fireOrderCallback(*ord, st);
    }

    if (orderToDelete) {
        orderToDelete->markRetired();
        HazardPointerManager::instance().retireNode(
            orderToDelete, &SafeDeleter::deleteOrder);
    }
}

void OrderBook::matchOrder(Order* order) {
    if (order->side == Side::BUY) {
        matchBuyOrder(order);
    } else {
        matchSellOrder(order);
    }
}

void OrderBook::matchBuyOrder(Order* buyOrder) {
    while (!buyOrder->isFilled() && !askTree.isEmpty()) {
        RBNode* bestAsk = askTree.getMinimum();
        if (!bestAsk) break;

        if (bestAsk->price > buyOrder->price) {
            break;
        }

        OrderQueue* queue = static_cast<OrderQueue*>(bestAsk->queue);
        if (!queue || queue->empty()) break;

        Order* sellOrder = queue->front();
        if (!sellOrder) break;

        uint64_t tradeQty = std::min(buyOrder->getRemainingQuantity(),
                                    sellOrder->getRemainingQuantity());

        executeTrade(buyOrder, sellOrder, bestAsk->price, tradeQty);

        if (sellOrder->isFilled()) {
            queue->pop_front();
            orderMap.erase(sellOrder->orderId);
            orderSideMap.erase(sellOrder->orderId);
            updatePriceLevel(askTree, Side::SELL, bestAsk->price,
                           -static_cast<int64_t>(tradeQty), -1);
            sellOrder->markRetired();
            HazardPointerManager::instance().retireNode(
                sellOrder, &SafeDeleter::deleteOrder);
        } else {
            sellOrder->status = OrderStatus::PARTIAL_FILLED;
            updatePriceLevel(askTree, Side::SELL, bestAsk->price,
                           -static_cast<int64_t>(tradeQty), 0);
        }
    }
}

void OrderBook::matchSellOrder(Order* sellOrder) {
    while (!sellOrder->isFilled() && !bidTree.isEmpty()) {
        RBNode* bestBid = bidTree.getMaximum();
        if (!bestBid) break;

        if (bestBid->price < sellOrder->price) {
            break;
        }

        OrderQueue* queue = static_cast<OrderQueue*>(bestBid->queue);
        if (!queue || queue->empty()) break;

        Order* buyOrder = queue->front();
        if (!buyOrder) break;

        uint64_t tradeQty = std::min(sellOrder->getRemainingQuantity(),
                                    buyOrder->getRemainingQuantity());

        executeTrade(buyOrder, sellOrder, bestBid->price, tradeQty);

        if (buyOrder->isFilled()) {
            queue->pop_front();
            orderMap.erase(buyOrder->orderId);
            orderSideMap.erase(buyOrder->orderId);
            updatePriceLevel(bidTree, Side::BUY, bestBid->price,
                           -static_cast<int64_t>(tradeQty), -1);
            buyOrder->markRetired();
            HazardPointerManager::instance().retireNode(
                buyOrder, &SafeDeleter::deleteOrder);
        } else {
            buyOrder->status = OrderStatus::PARTIAL_FILLED;
            updatePriceLevel(bidTree, Side::BUY, bestBid->price,
                           -static_cast<int64_t>(tradeQty), 0);
        }
    }
}

void OrderBook::executeTrade(Order* buyOrder, Order* sellOrder,
                            int64_t price, uint64_t quantity) {
    buyOrder->filledQuantity += quantity;
    sellOrder->filledQuantity += quantity;

    Trade trade(++tradeIdCounter, buyOrder->orderId, sellOrder->orderId,
                price, quantity, symbol);

    fireTradeCallback(trade);
}

void OrderBook::addOrderToBook(Order* order) {
    RedBlackTree& tree = (order->side == Side::BUY) ? bidTree : askTree;
    OrderQueue* queue = getOrCreateQueue(tree, order->price);
    queue->push_back(order);

    updatePriceLevel(tree, order->side, order->price,
                    static_cast<int64_t>(order->getRemainingQuantity()), 1);

    orderMap[order->orderId] = order;
    orderSideMap[order->orderId] = order->side;
}

bool OrderBook::cancelOrder(uint64_t orderId) {
    Order* orderToRetire = nullptr;

    {
        WriteGuard guard(bookLock);

        auto it = orderMap.find(orderId);
        if (it == orderMap.end()) {
            return false;
        }

        Order* order = it->second;
        Side side = orderSideMap[orderId];
        int64_t price = order->price;
        RedBlackTree& tree = (side == Side::BUY) ? bidTree : askTree;

        OrderQueue* queue = getQueue(tree, price);
        if (queue) {
            queue->remove(orderId);
            updatePriceLevel(tree, side, price,
                            -static_cast<int64_t>(order->getRemainingQuantity()), -1);
        }

        order->status = OrderStatus::CANCELLED;
        orderMap.erase(it);
        orderSideMap.erase(orderId);

        orderToRetire = order;
        sequenceNumber++;
    }

    fireOrderCallback(*orderToRetire, OrderStatus::CANCELLED);

    orderToRetire->markRetired();
    HazardPointerManager::instance().retireNode(
        orderToRetire, &SafeDeleter::deleteOrder);

    return true;
}

Order* OrderBook::findOrder(uint64_t orderId) const {
    ReadGuard guard(bookLock);
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        return it->second;
    }
    return nullptr;
}

int64_t OrderBook::getBestBid() const {
    ReadGuard guard(bookLock);
    RBNode* maxBid = bidTree.getMaximum();
    return maxBid ? maxBid->price : 0;
}

int64_t OrderBook::getBestAsk() const {
    ReadGuard guard(bookLock);
    RBNode* minAsk = askTree.getMinimum();
    return minAsk ? minAsk->price : 0;
}

uint64_t OrderBook::getBestBidQuantity() const {
    ReadGuard guard(bookLock);
    RBNode* maxBid = bidTree.getMaximum();
    if (maxBid && maxBid->queue) {
        return static_cast<OrderQueue*>(maxBid->queue)->totalQuantity();
    }
    return 0;
}

uint64_t OrderBook::getBestAskQuantity() const {
    ReadGuard guard(bookLock);
    RBNode* minAsk = askTree.getMinimum();
    if (minAsk && minAsk->queue) {
        return static_cast<OrderQueue*>(minAsk->queue)->totalQuantity();
    }
    return 0;
}

size_t OrderBook::getBidCount() const {
    ReadGuard guard(bookLock);
    return bidTree.size();
}

size_t OrderBook::getAskCount() const {
    ReadGuard guard(bookLock);
    return askTree.size();
}

size_t OrderBook::getTotalOrderCount() const {
    ReadGuard guard(bookLock);
    return orderMap.size();
}

void OrderBook::collectPriceLevels(const RedBlackTree& tree,
                                  std::vector<PriceLevel>& levels,
                                  size_t depth, bool reverse) const {
    size_t count = 0;
    auto callback = [&](RBNode* node) {
        if (count >= depth) return;
        PriceLevel level;
        level.price = node->price;
        level.quantity = node->totalQuantity;
        level.orderCount = node->orderCount;
        levels.push_back(level);
        count++;
    };

    if (reverse) {
        tree.traverseReverseInOrder(callback);
    } else {
        tree.traverseInOrder(callback);
    }
}

OrderBookSnapshot OrderBook::getSnapshot(size_t depth) const {
    ReadGuard guard(bookLock);

    OrderBookSnapshot snapshot;
    snapshot.timestamp = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    snapshot.sequence = sequenceNumber;

    collectPriceLevels(bidTree, snapshot.bids, depth, true);
    collectPriceLevels(askTree, snapshot.asks, depth, false);

    return snapshot;
}

}
