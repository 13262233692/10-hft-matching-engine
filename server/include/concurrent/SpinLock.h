#pragma once

#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <thread>

namespace HFT {

class SpinLock {
public:
    SpinLock() : flag_(false) {}

    void lock() {
        while (flag_.exchange(true, std::memory_order_acquire)) {
            while (flag_.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
        }
    }

    bool tryLock() {
        return !flag_.exchange(true, std::memory_order_acquire);
    }

    void unlock() {
        flag_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> flag_;
};

class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock& lock) : lock_(lock), owns_(true) {
        lock_.lock();
    }

    ~SpinLockGuard() {
        if (owns_) lock_.unlock();
    }

    void unlock() {
        if (owns_) {
            lock_.unlock();
            owns_ = false;
        }
    }

private:
    SpinLock& lock_;
    bool owns_;
};

class RWSpinLock {
public:
    RWSpinLock() : readers_(0), writer_(false) {}

    void readLock() {
        while (true) {
            while (writer_.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            readers_.fetch_add(1, std::memory_order_acquire);
            if (!writer_.load(std::memory_order_acquire)) {
                return;
            }
            readers_.fetch_sub(1, std::memory_order_release);
        }
    }

    void readUnlock() {
        readers_.fetch_sub(1, std::memory_order_release);
    }

    void writeLock() {
        while (writer_.exchange(true, std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        while (readers_.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }
    }

    void writeUnlock() {
        writer_.store(false, std::memory_order_release);
    }

private:
    std::atomic<int32_t> readers_;
    std::atomic<bool> writer_;
};

class ReadGuard {
public:
    explicit ReadGuard(RWSpinLock& lock) : lock_(lock) { lock_.readLock(); }
    ~ReadGuard() { lock_.readUnlock(); }
private:
    RWSpinLock& lock_;
};

class WriteGuard {
public:
    explicit WriteGuard(RWSpinLock& lock) : lock_(lock) { lock_.writeLock(); }
    ~WriteGuard() { lock_.writeUnlock(); }
private:
    RWSpinLock& lock_;
};

}
