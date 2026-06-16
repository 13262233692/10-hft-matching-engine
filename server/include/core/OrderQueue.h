#pragma once

#include "core/Order.h"
#include <unordered_map>
#include <cstdint>

namespace HFT {

class OrderQueue {
public:
    OrderQueue();
    ~OrderQueue();

    void push_back(Order* order);

    Order* pop_front();

    Order* front() const { return head; }

    Order* back() const { return tail; }

    bool remove(uint64_t orderId);

    Order* find(uint64_t orderId) const;

    bool empty() const { return head == nullptr; }

    size_t size() const { return orderCount; }

    uint64_t totalQuantity() const { return totalQty; }

    void clear();

    Order* begin() const { return head; }

    Order* end() const { return nullptr; }

    static Order* next(Order* order) { return order ? order->next : nullptr; }

private:
    Order* head;
    Order* tail;
    size_t orderCount;
    uint64_t totalQty;
    std::unordered_map<uint64_t, Order*> orderMap;
};

}
