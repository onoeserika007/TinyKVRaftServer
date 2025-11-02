#include "scheduler.h"
#include "sync.h"
#include <iostream>
#include <atomic>
#include <vector>

using namespace fiber;

// 测试1：fiber::lock_guard基本功能
void test_lock_guard() {
    LOG_INFO("=== Test 1: fiber::lock_guard Basic Usage ===");
    
    static FiberMutex mtx;
    static int shared_value = 0;
    static std::atomic<int> access_count{0};
    const int num_fibers = 5;
    const int ops_per_fiber = 10;
    
    WaitGroup wg;
    wg.add(num_fibers);
    
    for (int i = 0; i < num_fibers; ++i) {
        Fiber::go([i, &wg]() {
            for (int j = 0; j < ops_per_fiber; ++j) {
                {
                    fiber::lock_guard<FiberMutex> lock(mtx);
                    int old_val = shared_value;
                    Fiber::yield();  // 让出CPU，测试是否真正持有锁
                    shared_value = old_val + 1;
                    access_count++;
                }
                // 锁在这里自动释放
                Fiber::yield();
            }
            LOG_INFO("Fiber {} completed all operations", i);
            wg.done();
        });
    }
    
    wg.wait();
    
    int expected = num_fibers * ops_per_fiber;
    if (shared_value == expected && access_count == expected) {
        LOG_INFO("✓ PASS: lock_guard test (value={}, accesses={})", shared_value, access_count.load());
    } else {
        LOG_ERROR("✗ FAIL: lock_guard test (expected={}, value={}, accesses={})", 
                  expected, shared_value, access_count.load());
    }
}

// 测试2：fiber::unique_lock基本功能
void test_unique_lock() {
    LOG_INFO("=== Test 2: fiber::unique_lock Basic Usage ===");
    
    static FiberMutex mtx;
    static int counter = 0;
    
    WaitGroup wg;
    wg.add(2);
    
    // Fiber 1: 使用unique_lock
    Fiber::go([&wg]() {
        fiber::unique_lock<FiberMutex> lock(mtx);
        LOG_INFO("Fiber 1: Acquired lock");
        counter++;
        Fiber::yield();
        counter++;
        LOG_INFO("Fiber 1: counter = {}", counter);
        lock.unlock();  // 提前释放锁
        LOG_INFO("Fiber 1: Unlocked");
        
        Fiber::sleep(50);  // 让Fiber 2有机会获取锁
        
        lock.lock();  // 重新获取锁
        LOG_INFO("Fiber 1: Re-acquired lock");
        counter++;
        LOG_INFO("Fiber 1: Final counter = {}", counter);
        wg.done();
    });
    
    // Fiber 2: 等待获取锁
    Fiber::go([&wg]() {
        Fiber::sleep(20);  // 确保Fiber 1先获取锁
        LOG_INFO("Fiber 2: Trying to acquire lock...");
        fiber::unique_lock<FiberMutex> lock(mtx);
        LOG_INFO("Fiber 2: Acquired lock, counter = {}", counter);
        counter += 10;
        LOG_INFO("Fiber 2: Updated counter = {}", counter);
        wg.done();
    });
    
    wg.wait();
    
    if (counter == 13) {  // 1 + 1 + 1 + 10
        LOG_INFO("✓ PASS: unique_lock test (counter={})", counter);
    } else {
        LOG_ERROR("✗ FAIL: unique_lock test (expected=13, got={})", counter);
    }
}

// 测试3：try_lock功能
void test_try_lock() {
    LOG_INFO("=== Test 3: FiberMutex::try_lock() ===");
    
    static FiberMutex mtx;
    static std::atomic<int> success_count{0};
    static std::atomic<int> fail_count{0};
    
    WaitGroup wg;
    wg.add(3);
    
    // Fiber 1: 持有锁一段时间
    Fiber::go([&wg]() {
        mtx.lock();
        LOG_INFO("Fiber 1: Locked, holding for 100ms");
        Fiber::sleep(100);
        mtx.unlock();
        LOG_INFO("Fiber 1: Unlocked");
        wg.done();
    });
    
    Fiber::sleep(10);  // 确保Fiber 1先获取锁
    
    // Fiber 2 & 3: 尝试获取锁
    for (int i = 2; i <= 3; ++i) {
        Fiber::go([i, &wg]() {
            LOG_INFO("Fiber {}: Trying try_lock()...", i);
            if (mtx.try_lock()) {
                success_count++;
                LOG_INFO("Fiber {}: try_lock() SUCCESS", i);
                Fiber::sleep(20);
                mtx.unlock();
                LOG_INFO("Fiber {}: Unlocked", i);
            } else {
                fail_count++;
                LOG_INFO("Fiber {}: try_lock() FAILED (expected)", i);
                
                // 等待后重试
                Fiber::sleep(120);
                if (mtx.try_lock()) {
                    success_count++;
                    LOG_INFO("Fiber {}: Retry try_lock() SUCCESS", i);
                    mtx.unlock();
                } else {
                    LOG_ERROR("Fiber {}: Retry try_lock() FAILED (unexpected)", i);
                }
            }
            wg.done();
        });
    }
    
    wg.wait();
    
    // 至少应该有2次失败（在Fiber 1持有锁时）和2次成功（重试时）
    if (fail_count >= 2 && success_count >= 2) {
        LOG_INFO("✓ PASS: try_lock test (success={}, fail={})", success_count.load(), fail_count.load());
    } else {
        LOG_ERROR("✗ FAIL: try_lock test (success={}, fail={})", success_count.load(), fail_count.load());
    }
}

// 测试4：unique_lock与try_lock结合  
void test_unique_lock_try() {
    LOG_INFO("=== Test 4: unique_lock::try_lock() ===");
    
    static FiberMutex mtx;
    static FiberMutex sync_mtx;
    static FiberCondition sync_cv;
    static bool fiber1_locked = false;
    static bool test_passed = true;
    
    WaitGroup wg;
    wg.add(2);
    
    // Fiber 1: 持有锁
    Fiber::go([&wg]() {
        mtx.lock();
        LOG_INFO("Fiber 1: Holding lock");
        
        // 通知Fiber 2
        {
            std::unique_lock<FiberMutex> lock(sync_mtx);
            fiber1_locked = true;
            sync_cv.notify_one();
        }
        
        Fiber::sleep(80);
        LOG_INFO("Fiber 1: Releasing lock");
        mtx.unlock();
        wg.done();
    });
    
    // Fiber 2: 直接使用mutex的try_lock
    Fiber::go([&wg, &test_passed]() {
        // 等待Fiber 1拿到锁
        {
            std::unique_lock<FiberMutex> lock(sync_mtx);
            while (!fiber1_locked) {
                sync_cv.wait(lock);
            }
        }
        
        LOG_INFO("Fiber 2: Trying to acquire lock with try_lock");
        
        if (mtx.try_lock()) {
            LOG_ERROR("Fiber 2: try_lock() should have failed!");
            test_passed = false;
            mtx.unlock();
        } else {
            LOG_INFO("Fiber 2: try_lock() failed as expected (lock held by Fiber 1)");
        }
        
        // 等待Fiber 1释放锁
        Fiber::sleep(100);
        
        if (mtx.try_lock()) {
            LOG_INFO("Fiber 2: try_lock() succeeded after Fiber 1 released");
            mtx.unlock();
        } else {
            LOG_ERROR("Fiber 2: try_lock() should have succeeded!");
            test_passed = false;
        }
        
        wg.done();
    });
    
    wg.wait();
    
    if (test_passed) {
        LOG_INFO("✓ PASS: unique_lock try_lock test");
    } else {
        LOG_ERROR("✗ FAIL: unique_lock try_lock test");
    }
}

// 测试5：高并发场景
void test_high_concurrency() {
    LOG_INFO("=== Test 5: High Concurrency Stress Test ===");
    
    static FiberMutex mtx;
    static int shared_counter = 0;
    static std::vector<int> operation_order;
    const int num_fibers = 20;
    const int ops_per_fiber = 50;
    
    WaitGroup wg;
    wg.add(num_fibers);
    
    for (int i = 0; i < num_fibers; ++i) {
        Fiber::go([i, &wg]() {
            for (int j = 0; j < ops_per_fiber; ++j) {
                fiber::lock_guard<FiberMutex> lock(mtx);
                shared_counter++;
                if (j % 10 == 0) {  // 每10次记录一次
                    operation_order.push_back(i);
                }
                // 随机yield
                if ((i * j) % 3 == 0) {
                    Fiber::yield();
                }
            }
            wg.done();
        });
    }
    
    wg.wait();
    
    int expected = num_fibers * ops_per_fiber;
    if (shared_counter == expected) {
        LOG_INFO("✓ PASS: High concurrency test (counter={}, operations={})", 
                 shared_counter, operation_order.size());
    } else {
        LOG_ERROR("✗ FAIL: High concurrency test (expected={}, got={})", 
                  expected, shared_counter);
    }
}

// 测试6：std::lock_guard兼容性（应该可以用std::lock_guard）
void test_std_lock_guard_compatibility() {
    LOG_INFO("=== Test 6: std::lock_guard Compatibility ===");
    
    static FiberMutex mtx;
    static int value = 0;
    
    WaitGroup wg;
    wg.add(3);
    
    for (int i = 0; i < 3; ++i) {
        Fiber::go([i, &wg]() {
            {
                std::lock_guard<FiberMutex> lock(mtx);  // 使用std::lock_guard
                value += (i + 1) * 10;
                Fiber::yield();
                LOG_INFO("Fiber {}: value = {}", i, value);
            }
            wg.done();
        });
    }
    
    wg.wait();
    
    if (value == 60) {  // 10 + 20 + 30
        LOG_INFO("✓ PASS: std::lock_guard compatibility test");
    } else {
        LOG_ERROR("✗ FAIL: std::lock_guard compatibility (expected=60, got={})", value);
    }
}

FIBER_MAIN() {
    LOG_INFO("========================================");
    LOG_INFO("    FiberMutex Comprehensive Test");
    LOG_INFO("========================================");
    
    try {
        test_lock_guard();
        Fiber::sleep(100);
        
        test_unique_lock();
        Fiber::sleep(100);
        
        test_try_lock();
        Fiber::sleep(100);
        
        test_unique_lock_try();
        Fiber::sleep(100);
        
        test_high_concurrency();
        Fiber::sleep(100);
        
        test_std_lock_guard_compatibility();
        
        LOG_INFO("");
        LOG_INFO("========================================");
        LOG_INFO("    All Mutex Tests Completed");
        LOG_INFO("========================================");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        return 1;
    }
    
    return 0;
}
