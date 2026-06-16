#pragma once

#include "core/Order.h"
#include <cstdint>
#include <functional>

namespace HFT {

enum class RBColor : uint8_t {
    RED = 0,
    BLACK = 1
};

struct RBNode {
    int64_t price;
    uint64_t totalQuantity;
    uint32_t orderCount;
    RBColor color;
    RBNode* parent;
    RBNode* left;
    RBNode* right;
    void* queue;

    RBNode(int64_t p)
        : price(p), totalQuantity(0), orderCount(0), color(RBColor::RED),
          parent(nullptr), left(nullptr), right(nullptr), queue(nullptr) {}
};

class RedBlackTree {
public:
    using TraverseCallback = std::function<void(RBNode*)>;

    RedBlackTree();
    ~RedBlackTree();

    RBNode* insert(int64_t price);

    RBNode* find(int64_t price);

    void remove(int64_t price);

    RBNode* getMinimum() const;

    RBNode* getMaximum() const;

    RBNode* getSuccessor(RBNode* node) const;

    RBNode* getPredecessor(RBNode* node) const;

    void traverseInOrder(TraverseCallback callback) const;

    void traverseReverseInOrder(TraverseCallback callback) const;

    size_t size() const { return nodeCount; }

    bool isEmpty() const { return root == nullptr; }

    void clear();

private:
    RBNode* root;
    size_t nodeCount;

    void rotateLeft(RBNode* x);
    void rotateRight(RBNode* x);
    void insertFixup(RBNode* z);
    void deleteFixup(RBNode* x);
    void transplant(RBNode* u, RBNode* v);
    RBNode* minimum(RBNode* node) const;
    RBNode* maximum(RBNode* node) const;
    void clearRecursive(RBNode* node);
    void traverseInOrderRecursive(RBNode* node, TraverseCallback& callback) const;
    void traverseReverseInOrderRecursive(RBNode* node, TraverseCallback& callback) const;
};

}
