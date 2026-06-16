#include "core/OrderQueue.h"
#include "concurrent/HazardPointer.h"

namespace HFT {

OrderQueue::OrderQueue()
    : head(nullptr), tail(nullptr), orderCount(0), totalQty(0) {}

OrderQueue::~OrderQueue() {
    clear();
}

void OrderQueue::clear() {
    WriteGuard guard(queueLock);
    Order* current = head;
    while (current) {
        Order* nextPtr = current->next;
        current->prev = nullptr;
        current->next = nullptr;
        if (!current->isRetired()) {
            current->markRetired();
        }
        current = nextPtr;
    }
    head = nullptr;
    tail = nullptr;
    orderCount = 0;
    totalQty = 0;
    orderMap.clear();
}

void OrderQueue::push_back(Order* order) {
    if (!order) return;

    WriteGuard guard(queueLock);

    order->prev = tail;
    order->next = nullptr;

    if (tail) {
        tail->next = order;
    } else {
        head = order;
    }
    tail = order;

    orderCount++;
    totalQty += order->getRemainingQuantity();
    orderMap[order->orderId] = order;
}

Order* OrderQueue::pop_front() {
    WriteGuard guard(queueLock);

    if (!head) return nullptr;

    Order* order = head;
    head = head->next;

    if (head) {
        head->prev = nullptr;
    } else {
        tail = nullptr;
    }

    order->prev = nullptr;
    order->next = nullptr;

    orderCount--;
    totalQty -= order->getRemainingQuantity();
    orderMap.erase(order->orderId);

    return order;
}

bool OrderQueue::remove(uint64_t orderId) {
    WriteGuard guard(queueLock);

    auto it = orderMap.find(orderId);
    if (it == orderMap.end()) {
        return false;
    }

    Order* order = it->second;

    if (order->prev) {
        order->prev->next = order->next;
    } else {
        head = order->next;
    }

    if (order->next) {
        order->next->prev = order->prev;
    } else {
        tail = order->prev;
    }

    totalQty -= order->getRemainingQuantity();
    orderCount--;
    orderMap.erase(it);

    order->prev = nullptr;
    order->next = nullptr;

    return true;
}

bool OrderQueue::safeRemove(uint64_t orderId) {
    WriteGuard guard(queueLock);

    auto it = orderMap.find(orderId);
    if (it == orderMap.end()) {
        return false;
    }

    Order* order = it->second;

    if (order->prev) {
        order->prev->next = order->next;
    } else {
        head = order->next;
    }

    if (order->next) {
        order->next->prev = order->prev;
    } else {
        tail = order->prev;
    }

    totalQty -= order->getRemainingQuantity();
    orderCount--;
    orderMap.erase(it);

    order->markRetired();
    order->prev = nullptr;
    order->next = nullptr;

    HazardPointerManager::instance().retireNode(
        order, &SafeDeleter::deleteOrder);

    return true;
}

Order* OrderQueue::safeNext(Order* current) const {
    while (current) {
        current = current->next;
        if (!current || !current->isRetired()) {
            return current;
        }
    }
    return nullptr;
}

Order* OrderQueue::find(uint64_t orderId) const {
    ReadGuard guard(queueLock);
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        return it->second;
    }
    return nullptr;
}

}
