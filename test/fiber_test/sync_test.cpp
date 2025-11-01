#include "scheduler.h"
#include "sync.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

using namespace fiber;

// 测试FiberMutex基本功能
void test_mutex_basic() {
    LOG_INFO("=== Testing FiberMutex Basic Operations ===");
    
    static FiberMutex mtx;
    static std::atomic<int> shared_counter{0};
    const int num_fibers = 3;
    const int increments_per_fiber = 10;
    WaitGroup wg;
    wg.add(num_fibers * increments_per_fiber);
    
    // 使用正确的用户接口：Fiber::go()
    for (int i = 0; i < num_fibers; ++i) {
        Fiber::go([i, increments_per_fiber, &wg]() {
            for (int j = 0; j < increments_per_fiber; ++j) {
                fiber_lock_guard lock(mtx);
                int old_val = shared_counter.load();
                // 模拟一些工作
                Fiber::yield();
                shared_counter.store(old_val + 1);

                LOG_INFO("Fiber {} incremented counter to {}", i , shared_counter.load());
                wg.done();
            }
            LOG_INFO("Fiber {} completed", i);
        });
    }

    wg.wait();
    
    int expected = num_fibers * increments_per_fiber;
    LOG_INFO("Expected counter {}, Actual counter: {}", expected, shared_counter.load());

    if (shared_counter.load() == expected) {
        LOG_INFO("✓ FiberMutex basic test PASSED");
    } else {
        LOG_INFO("✗ FiberMutex basic test FAILED");
    }
}

// 简单的WaitGroup测试
void test_wait_group() {
    LOG_INFO("=== Testing WaitGroup ===");
    
    static WaitGroup wg;
    static std::atomic<int> completed_tasks{0};
    const int num_workers = 2;

    wg.add(num_workers);
    
    // 工作协程
    for (int i = 0; i < num_workers; ++i) {
        
        Fiber::go([i]() {
            LOG_INFO("Worker {}: Starting work...", i);
            
            // 模拟工作
            for (int j = 0; j < 3; ++j) {
                Fiber::yield();
                completed_tasks++;
            }

            LOG_INFO("Worker {}: Work completed.", i);
            wg.done(); // 完成一个任务
        });
    }

    LOG_INFO("Main: Waiting for all workers to complete...");
    wg.wait();
    LOG_INFO("Main: All workers completed! Total tasks: {}", completed_tasks.load());

    if (completed_tasks.load() == num_workers * 3) {
        LOG_INFO("✓ WaitGroup test PASSED");
        std::cout << "✓ WaitGroup test PASSED" << std::endl;
    } else {
        LOG_INFO("✗ WaitGroup test FAILED (expected {}, got {})", num_workers * 3, completed_tasks.load());
    }
}

FIBER_MAIN() {
    LOG_INFO("Starting Fiber Synchronization Tests...");
    try {
        test_wait_group();
        // test_mutex_basic();

        LOG_INFO("=== All Tests Completed ===");

    } catch (const std::exception& e) {
        LOG_INFO("Test failed with exception: {}", e.what());
        return 1;
    }
    return 0;
}