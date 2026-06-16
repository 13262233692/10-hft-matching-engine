#include "core/OrderQueue.h"

namespace HFT {

OrderQueue::OrderQueue()
    : head(nullptr), tail(nullptr), orderCount(0), totalQty(0) {}

OrderQueue::~OrderQueue() {
    clear();
}

void OrderQueue::clear() {
    Order* current = head;
    while (current) {
        Order* next = current->next;
        current->prev = nullptr;
        current->next = nullptr;
        current = next;
    }
    head = nullptr;
    tail = nullptr;
    orderCount = 0;
    totalQty = 0;
    orderMap.clear();
}

void OrderQueue::push_back(Order* order) {
    if (!order) return;

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

Order* OrderQueue::find(uint64_t orderId) const {
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        return it->second;
    }
    return nullptr;
}

}
