#pragma once

#include <atomic>
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include <mutex>
#include "concurrent/SpinLock.h"

namespace HFT {

constexpr size_t HAZARD_POINTER_COUNT = 128;
constexpr size_t MAX_RETIRE_BATCH = 256;

struct HazardPointer {
    std::atomic<const void*> pointer;
    std::atomic<uint64_t> ownerId;

    HazardPointer() : pointer(nullptr), ownerId(0) {}

    void publish(const void* ptr, uint64_t owner) {
        ownerId.store(owner, std::memory_order_release);
        pointer.store(ptr, std::memory_order_release);
    }

    void clear() {
        pointer.store(nullptr, std::memory_order_release);
    }
};

class HazardPointerManager {
public:
    static HazardPointerManager& instance() {
        static HazardPointerManager mgr;
        return mgr;
    }

    uint64_t registerThread() {
        return threadCounter_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    void unregisterThread(uint64_t ownerId) {
        for (size_t i = 0; i < HAZARD_POINTER_COUNT; ++i) {
            uint64_t owner = hazardPointers_[i].ownerId.load(std::memory_order_acquire);
            if (owner == ownerId) {
                hazardPointers_[i].clear();
                hazardPointers_[i].ownerId.store(0, std::memory_order_release);
            }
        }
    }

    HazardPointer* acquireHazardPointer(uint64_t ownerId) {
        for (size_t i = 0; i < HAZARD_POINTER_COUNT; ++i) {
            uint64_t expected = 0;
            if (hazardPointers_[i].ownerId.compare_exchange_strong(
                    expected, ownerId,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                return &hazardPointers_[i];
            }
        }
        return nullptr;
    }

    void retireNode(const void* ptr, void (*deleter)(const void*)) {
        RetiredNode rn;
        rn.pointer = ptr;
        rn.deleter = deleter;

        SpinLockGuard guard(retiredListLock_);
        retiredList_.push_back(rn);
    }

    void scanAndReclaim() {
        std::vector<const void*> protectedPtrs;
        protectedPtrs.reserve(HAZARD_POINTER_COUNT);

        for (size_t i = 0; i < HAZARD_POINTER_COUNT; ++i) {
            const void* p = hazardPointers_[i].pointer.load(std::memory_order_acquire);
            if (p != nullptr) {
                protectedPtrs.push_back(p);
            }
        }

        std::vector<RetiredNode> stillRetired;
        std::vector<RetiredNode> toDelete;

        {
            SpinLockGuard guard(retiredListLock_);
            toDelete.swap(retiredList_);
        }

        stillRetired.reserve(toDelete.size() / 2);
        for (auto& rn : toDelete) {
            bool isProtected = false;
            for (const void* p : protectedPtrs) {
                if (p == rn.pointer) {
                    isProtected = true;
                    break;
                }
            }
            if (!isProtected) {
                rn.deleter(rn.pointer);
            } else {
                stillRetired.push_back(rn);
            }
        }

        if (!stillRetired.empty()) {
            SpinLockGuard guard(retiredListLock_);
            retiredList_.insert(retiredList_.end(),
                               std::make_move_iterator(stillRetired.begin()),
                               std::make_move_iterator(stillRetired.end()));
        }
    }

    size_t tryScanAndReclaimIfFull() {
        size_t currentSize;
        {
            SpinLockGuard guard(retiredListLock_);
            currentSize = retiredList_.size();
        }
        if (currentSize >= MAX_RETIRE_BATCH) {
            scanAndReclaim();
        }
        return currentSize;
    }

    bool isHazardous(const void* ptr) const {
        for (size_t i = 0; i < HAZARD_POINTER_COUNT; ++i) {
            if (hazardPointers_[i].pointer.load(std::memory_order_acquire) == ptr) {
                return true;
            }
        }
        return false;
    }

    size_t retiredCount() const {
        SpinLockGuard guard(retiredListLock_);
        return retiredList_.size();
    }

private:
    HazardPointerManager() = default;

    struct RetiredNode {
        const void* pointer;
        void (*deleter)(const void*);
    };

    std::array<HazardPointer, HAZARD_POINTER_COUNT> hazardPointers_;
    std::vector<RetiredNode> retiredList_;
    mutable SpinLock retiredListLock_;
    std::atomic<uint64_t> threadCounter_{0};
};

class HazardGuard {
public:
    HazardGuard(uint64_t ownerId)
        : ownerId_(ownerId), hp_(nullptr) {
        hp_ = HazardPointerManager::instance().acquireHazardPointer(ownerId_);
    }

    ~HazardGuard() {
        if (hp_) {
            hp_->clear();
        }
    }

    void protect(const void* ptr) {
        if (hp_) {
            hp_->publish(ptr, ownerId_);
        }
    }

    void clear() {
        if (hp_) {
            hp_->clear();
        }
    }

    HazardPointer* get() { return hp_; }

private:
    HazardGuard(const HazardGuard&) = delete;
    HazardGuard& operator=(const HazardGuard&) = delete;

    uint64_t ownerId_;
    HazardPointer* hp_;
};

struct SafeDeleter {
    static void deleteOrder(const void* ptr) {
        const Order* order = static_cast<const Order*>(ptr);
        delete order;
    }
};

}
