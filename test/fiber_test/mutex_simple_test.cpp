#include "scheduler.h"
#include "sync.h"
#include <iostream>
#include <atomic>

using namespace fiber;

// 简单的lock_guard测试
void test_simple_lock_guard() {
    LOG_INFO("=== Simple lock_guard Test ===");
    
    static FiberMutex mtx;
    static int value = 0;
    
    WaitGroup wg;
    wg.add(3);
    
    for (int i = 0; i < 3; ++i) {
        Fiber::go([i, &wg]() {
            LOG_INFO("Fiber {} starting", i);
            {
                fiber::lock_guard<FiberMutex> lock(mtx);
                LOG_INFO("Fiber {} acquired lock", i);
                value++;
                Fiber::sleep(10);  // 短暂等待
                LOG_INFO("Fiber {} releasing lock, value={}", i, value);
            }
            LOG_INFO("Fiber {} done", i);
            wg.done();
        });
    }
    
    LOG_INFO("Main: waiting for all fibers...");
    wg.wait();
    LOG_INFO("Main: all fibers completed, value={}", value);
    
    if (value == 3) {
        LOG_INFO("✓ PASS: Simple lock_guard test");
    } else {
        LOG_ERROR("✗ FAIL: Expected 3, got {}", value);
    }
}

// 测试std::lock_guard
void test_std_lock_guard() {
    LOG_INFO("=== std::lock_guard Test ===");
    
    static FiberMutex mtx;
    static int counter = 0;
    
    WaitGroup wg;
    wg.add(2);
    
    for (int i = 0; i < 2; ++i) {
        Fiber::go([i, &wg]() {
            LOG_INFO("Fiber {} starting", i);
            {
                std::lock_guard<FiberMutex> lock(mtx);  // 使用std::lock_guard
                LOG_INFO("Fiber {} acquired lock", i);
                counter += 10;
                Fiber::sleep(10);
                LOG_INFO("Fiber {} releasing lock, counter={}", i, counter);
            }
            wg.done();
        });
    }
    
    wg.wait();
    LOG_INFO("Counter final value: {}", counter);
    
    if (counter == 20) {
        LOG_INFO("✓ PASS: std::lock_guard test");
    } else {
        LOG_ERROR("✗ FAIL: Expected 20, got {}", counter);
    }
}

// 测试unique_lock
void test_simple_unique_lock() {
    LOG_INFO("=== Simple unique_lock Test ===");
    
    static FiberMutex mtx;
    static int value = 0;
    
    WaitGroup wg;
    wg.add(1);
    
    Fiber::go([&wg]() {
        LOG_INFO("Testing unique_lock lock/unlock");
        fiber::unique_lock<FiberMutex> lock(mtx);
        LOG_INFO("Acquired lock");
        value = 100;
        
        lock.unlock();
        LOG_INFO("Unlocked");
        
        Fiber::sleep(20);
        
        lock.lock();
        LOG_INFO("Re-acquired lock");
        value = 200;
        
        LOG_INFO("Final value: {}", value);
        wg.done();
    });
    
    wg.wait();
    
    if (value == 200) {
        LOG_INFO("✓ PASS: unique_lock test");
    } else {
        LOG_ERROR("✗ FAIL: Expected 200, got {}", value);
    }
}

// 测试try_lock基本功能
void test_simple_try_lock() {
    LOG_INFO("=== Simple try_lock Test ===");
    
    static FiberMutex mtx;
    static FiberMutex sync_mtx;
    static FiberCondition sync_cv;
    static bool fiber1_has_lock = false;
    static bool test_passed = true;
    
    WaitGroup wg;
    wg.add(2);
    
    // Fiber 1: 持有锁
    Fiber::go([&wg]() {
        LOG_INFO("Fiber 1: Locking");
        mtx.lock();
        LOG_INFO("Fiber 1: Locked");
        
        // 通知Fiber 2：我已经拿到锁了
        {
            std::unique_lock<FiberMutex> lock(sync_mtx);
            fiber1_has_lock = true;
            sync_cv.notify_one();
        }
        
        LOG_INFO("Fiber 1: Holding lock for 50ms");
        Fiber::sleep(50);
        LOG_INFO("Fiber 1: Unlocking");
        mtx.unlock();
        wg.done();
    });
    
    // Fiber 2: 尝试获取锁
    Fiber::go([&wg, test_passed = &test_passed]() {
        // 等待Fiber 1确认已拿到锁
        {
            std::unique_lock<FiberMutex> lock(sync_mtx);
            while (!fiber1_has_lock) {
                sync_cv.wait(lock);
            }
        }
        
        LOG_INFO("Fiber 2: Fiber 1 has lock, trying try_lock (should fail)");
        if (mtx.try_lock()) {
            LOG_ERROR("Fiber 2: try_lock should have failed!");
            *test_passed = false;
            mtx.unlock();
        } else {
            LOG_INFO("Fiber 2: try_lock failed as expected ✓");
        }
        
        // 等待Fiber 1释放锁
        LOG_INFO("Fiber 2: Waiting for Fiber 1 to release lock");
        Fiber::sleep(70);
        
        LOG_INFO("Fiber 2: Trying try_lock again (should succeed)");
        if (mtx.try_lock()) {
            LOG_INFO("Fiber 2: try_lock succeeded ✓");
            mtx.unlock();
        } else {
            LOG_ERROR("Fiber 2: try_lock should have succeeded!");
            *test_passed = false;
        }
        wg.done();
    });
    
    wg.wait();
    
    if (test_passed) {
        LOG_INFO("✓ PASS: try_lock test");
    } else {
        LOG_ERROR("✗ FAIL: try_lock test");
    }
}

FIBER_MAIN() {
    LOG_INFO("========================================");
    LOG_INFO("  Simple Mutex Tests (Debug Mode)");
    LOG_INFO("========================================");
    
    try {
        test_simple_lock_guard();
        LOG_INFO("");
        Fiber::sleep(100);
        
        test_std_lock_guard();
        LOG_INFO("");
        Fiber::sleep(100);
        
        test_simple_unique_lock();
        LOG_INFO("");
        Fiber::sleep(100);
        
        test_simple_try_lock();
        LOG_INFO("");
        
        LOG_INFO("========================================");
        LOG_INFO("  All Simple Tests Completed");
        LOG_INFO("========================================");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        return 1;
    }
    
    return 0;
}
