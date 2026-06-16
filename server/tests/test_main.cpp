#include <iostream>
#include <cassert>
#include <chrono>
#include <iomanip>

int testOrderBook();
int testFIXParser();
int testPerformance();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  HFT Matching Engine Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    int failed = 0;

    std::cout << "\n[TEST] Running OrderBook tests..." << std::endl;
    failed += testOrderBook();

    std::cout << "\n[TEST] Running FIXParser tests..." << std::endl;
    failed += testFIXParser();

    std::cout << "\n[TEST] Running Performance tests..." << std::endl;
    failed += testPerformance();

    std::cout << "\n========================================" << std::endl;
    if (failed == 0) {
        std::cout << "  ALL TESTS PASSED!" << std::endl;
    } else {
        std::cout << "  " << failed << " TEST(S) FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return failed;
}
