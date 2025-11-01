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

// 测试FiberCondition超时等待
void test_condition_timeout() {
    LOG_INFO("=== Testing FiberCondition Timeout ===");
    
    static FiberMutex mtx;
    static FiberCondition cond;
    static std::atomic<bool> ready{false};
    
    // 测试1：超时场景
    LOG_INFO("Test 1: Wait timeout (should timeout after 200ms)");
    Fiber::go([]() {
        std::unique_lock<FiberMutex> lock(mtx);
        auto start = std::chrono::steady_clock::now();
        
        // 等待200ms，不会被notify，应该超时
        bool result = cond.wait_for(lock, std::chrono::milliseconds(200));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (!result && elapsed >= 150 && elapsed <= 300) {
            LOG_INFO("PASS: Condition wait timed out correctly ({}ms)", elapsed);
        } else {
            LOG_ERROR("FAIL: Timeout behavior incorrect (result={}, elapsed={}ms)", result, elapsed);
        }
    });
    
    Fiber::sleep(400);
    
    // 测试2：被notify唤醒场景
    LOG_INFO("Test 2: Wait with notify (should wake up before timeout)");
    ready.store(false, std::memory_order_release);
    
    Fiber::go([]() {
        std::unique_lock<FiberMutex> lock(mtx);
        ready.store(true, std::memory_order_release);  // 标记已开始等待
        auto start = std::chrono::steady_clock::now();
        
        // 等待500ms，但会在200ms时被notify
        bool result = cond.wait_for(lock, std::chrono::milliseconds(500));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (result && elapsed < 400) {
            LOG_INFO("PASS: Woken up by notify ({}ms)", elapsed);
        } else {
            LOG_ERROR("FAIL: Should be woken by notify (result={}, elapsed={}ms)", result, elapsed);
        }
    });
    
    // 等待fiber进入wait状态
    while (!ready.load(std::memory_order_acquire)) {
        Fiber::yield();
    }
    Fiber::sleep(100);  // 额外等待确保进入wait
    
    // 发送notify
    cond.notify_one();
    
    Fiber::sleep(400);
    LOG_INFO("Condition timeout test completed");
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
        LOG_INFO("PASS: WaitGroup test");
    } else {
        LOG_ERROR("FAIL: WaitGroup test (expected {}, got {})", num_workers * 3, completed_tasks.load());
    }
}

FIBER_MAIN() {
    LOG_INFO("Starting Fiber Synchronization Tests...");
    try {
        test_wait_group();
        // test_mutex_basic();
        test_condition_timeout();

        LOG_INFO("=== All Tests Completed ===");

    } catch (const std::exception& e) {
        LOG_INFO("Test failed with exception: {}", e.what());
        return 1;
    }
    return 0;
}