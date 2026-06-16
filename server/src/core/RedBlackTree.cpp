#include "core/RedBlackTree.h"
#include "core/OrderQueue.h"

namespace HFT {

RedBlackTree::RedBlackTree() : root(nullptr), nodeCount(0) {}

RedBlackTree::~RedBlackTree() {
    clear();
}

void RedBlackTree::clear() {
    clearRecursive(root);
    root = nullptr;
    nodeCount = 0;
}

void RedBlackTree::clearRecursive(RBNode* node) {
    if (node) {
        clearRecursive(node->left);
        clearRecursive(node->right);
        if (node->queue) {
            delete static_cast<OrderQueue*>(node->queue);
            node->queue = nullptr;
        }
        delete node;
    }
}

void RedBlackTree::rotateLeft(RBNode* x) {
    RBNode* y = x->right;
    x->right = y->left;

    if (y->left) {
        y->left->parent = x;
    }

    y->parent = x->parent;

    if (!x->parent) {
        root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    y->left = x;
    x->parent = y;
}

void RedBlackTree::rotateRight(RBNode* x) {
    RBNode* y = x->left;
    x->left = y->right;

    if (y->right) {
        y->right->parent = x;
    }

    y->parent = x->parent;

    if (!x->parent) {
        root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }

    y->right = x;
    x->parent = y;
}

void RedBlackTree::insertFixup(RBNode* z) {
    while (z->parent && z->parent->color == RBColor::RED) {
        if (z->parent->parent && z->parent == z->parent->parent->left) {
            RBNode* y = z->parent->parent->right;
            if (y && y->color == RBColor::RED) {
                z->parent->color = RBColor::BLACK;
                y->color = RBColor::BLACK;
                z->parent->parent->color = RBColor::RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rotateLeft(z);
                }
                if (z->parent) z->parent->color = RBColor::BLACK;
                if (z->parent && z->parent->parent) {
                    z->parent->parent->color = RBColor::RED;
                    rotateRight(z->parent->parent);
                }
            }
        } else {
            RBNode* y = nullptr;
            if (z->parent && z->parent->parent) {
                y = z->parent->parent->left;
            }
            if (y && y->color == RBColor::RED) {
                z->parent->color = RBColor::BLACK;
                y->color = RBColor::BLACK;
                if (z->parent && z->parent->parent) {
                    z->parent->parent->color = RBColor::RED;
                    z = z->parent->parent;
                }
            } else {
                if (z->parent && z == z->parent->left) {
                    z = z->parent;
                    rotateRight(z);
                }
                if (z->parent) z->parent->color = RBColor::BLACK;
                if (z->parent && z->parent->parent) {
                    z->parent->parent->color = RBColor::RED;
                    rotateLeft(z->parent->parent);
                }
            }
        }
    }
    root->color = RBColor::BLACK;
}

RBNode* RedBlackTree::insert(int64_t price) {
    RBNode* y = nullptr;
    RBNode* x = root;

    while (x) {
        y = x;
        if (price < x->price) {
            x = x->left;
        } else if (price > x->price) {
            x = x->right;
        } else {
            return x;
        }
    }

    RBNode* z = new RBNode(price);
    z->parent = y;

    if (!y) {
        root = z;
    } else if (price < y->price) {
        y->left = z;
    } else {
        y->right = z;
    }

    z->left = nullptr;
    z->right = nullptr;
    z->color = RBColor::RED;

    insertFixup(z);
    nodeCount++;
    return z;
}

RBNode* RedBlackTree::find(int64_t price) {
    RBNode* x = root;
    while (x) {
        if (price < x->price) {
            x = x->left;
        } else if (price > x->price) {
            x = x->right;
        } else {
            return x;
        }
    }
    return nullptr;
}

void RedBlackTree::transplant(RBNode* u, RBNode* v) {
    if (!u->parent) {
        root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    if (v) {
        v->parent = u->parent;
    }
}

RBNode* RedBlackTree::minimum(RBNode* node) const {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

RBNode* RedBlackTree::maximum(RBNode* node) const {
    while (node && node->right) {
        node = node->right;
    }
    return node;
}

RBNode* RedBlackTree::getMinimum() const {
    return minimum(root);
}

RBNode* RedBlackTree::getMaximum() const {
    return maximum(root);
}

RBNode* RedBlackTree::getSuccessor(RBNode* node) const {
    if (!node) return nullptr;
    if (node->right) {
        return minimum(node->right);
    }
    RBNode* y = node->parent;
    while (y && node == y->right) {
        node = y;
        y = y->parent;
    }
    return y;
}

RBNode* RedBlackTree::getPredecessor(RBNode* node) const {
    if (!node) return nullptr;
    if (node->left) {
        return maximum(node->left);
    }
    RBNode* y = node->parent;
    while (y && node == y->left) {
        node = y;
        y = y->parent;
    }
    return y;
}

void RedBlackTree::deleteFixup(RBNode* x) {
    while (x != root && x && x->color == RBColor::BLACK) {
        if (x->parent && x == x->parent->left) {
            RBNode* w = x->parent ? x->parent->right : nullptr;
            if (w && w->color == RBColor::RED) {
                w->color = RBColor::BLACK;
                if (x->parent) x->parent->color = RBColor::RED;
                rotateLeft(x->parent);
                w = x->parent ? x->parent->right : nullptr;
            }
            bool leftBlack = w && w->left ? w->left->color == RBColor::BLACK : true;
            bool rightBlack = w && w->right ? w->right->color == RBColor::BLACK : true;
            if (w && leftBlack && rightBlack) {
                w->color = RBColor::RED;
                x = x->parent;
            } else if (w) {
                if (w->right && w->right->color == RBColor::BLACK) {
                    if (w->left) w->left->color = RBColor::BLACK;
                    w->color = RBColor::RED;
                    rotateRight(w);
                    w = x->parent ? x->parent->right : nullptr;
                }
                if (w) {
                    w->color = x->parent ? x->parent->color : RBColor::BLACK;
                    if (x->parent) x->parent->color = RBColor::BLACK;
                    if (w->right) w->right->color = RBColor::BLACK;
                    rotateLeft(x->parent);
                }
                x = root;
            } else {
                x = x->parent;
            }
        } else {
            RBNode* w = x->parent ? x->parent->left : nullptr;
            if (w && w->color == RBColor::RED) {
                w->color = RBColor::BLACK;
                if (x->parent) x->parent->color = RBColor::RED;
                rotateRight(x->parent);
                w = x->parent ? x->parent->left : nullptr;
            }
            bool rightBlack = w && w->right ? w->right->color == RBColor::BLACK : true;
            bool leftBlack = w && w->left ? w->left->color == RBColor::BLACK : true;
            if (w && rightBlack && leftBlack) {
                w->color = RBColor::RED;
                x = x->parent;
            } else if (w) {
                if (w->left && w->left->color == RBColor::BLACK) {
                    if (w->right) w->right->color = RBColor::BLACK;
                    w->color = RBColor::RED;
                    rotateLeft(w);
                    w = x->parent ? x->parent->left : nullptr;
                }
                if (w) {
                    w->color = x->parent ? x->parent->color : RBColor::BLACK;
                    if (x->parent) x->parent->color = RBColor::BLACK;
                    if (w->left) w->left->color = RBColor::BLACK;
                    rotateRight(x->parent);
                }
                x = root;
            } else {
                x = x->parent;
            }
        }
    }
    if (x) x->color = RBColor::BLACK;
}

void RedBlackTree::remove(int64_t price) {
    RBNode* z = find(price);
    if (!z) return;

    RBNode* y = z;
    RBNode* x = nullptr;
    RBColor yOriginalColor = y->color;

    if (!z->left) {
        x = z->right;
        transplant(z, z->right);
    } else if (!z->right) {
        x = z->left;
        transplant(z, z->left);
    } else {
        y = minimum(z->right);
        yOriginalColor = y->color;
        x = y->right;
        if (y->parent == z) {
            if (x) x->parent = y;
        } else {
            transplant(y, y->right);
            y->right = z->right;
            if (y->right) y->right->parent = y;
        }
        transplant(z, y);
        y->left = z->left;
        if (y->left) y->left->parent = y;
        y->color = z->color;
    }

    if (yOriginalColor == RBColor::BLACK) {
        deleteFixup(x);
    }

    if (z->queue) {
        delete static_cast<OrderQueue*>(z->queue);
    }
    delete z;
    nodeCount--;
}

void RedBlackTree::traverseInOrder(TraverseCallback callback) const {
    traverseInOrderRecursive(root, callback);
}

void RedBlackTree::traverseReverseInOrder(TraverseCallback callback) const {
    traverseReverseInOrderRecursive(root, callback);
}

void RedBlackTree::traverseInOrderRecursive(RBNode* node, TraverseCallback& callback) const {
    if (node) {
        traverseInOrderRecursive(node->left, callback);
        callback(node);
        traverseInOrderRecursive(node->right, callback);
    }
}

void RedBlackTree::traverseReverseInOrderRecursive(RBNode* node, TraverseCallback& callback) const {
    if (node) {
        traverseReverseInOrderRecursive(node->right, callback);
        callback(node);
        traverseReverseInOrderRecursive(node->left, callback);
    }
}

}
