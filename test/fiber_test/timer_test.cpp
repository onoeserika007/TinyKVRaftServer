#include "scheduler.h"
#include "fiber.h"
#include "timer.h"
#include "sync.h"
#include "logger.h"
#include <iostream>
#include <atomic>
#include <chrono>

using namespace fiber;

// 测试基本定时器功能
void test_basic_timer() {
    LOG_INFO("=== Test 1: Basic Timer ===");
    
    std::atomic<int> counter{0};
    std::atomic<bool> timer_executed{false};
    
    auto& timer_wheel = TimerWheel::getInstance();
    
    // 添加一个200ms的定时器
    auto timer = timer_wheel.addTimer(static_cast<uint64_t>(200), [&]() {
        counter++;
        timer_executed.store(true);
        LOG_INFO("Timer executed! Counter: {}", counter.load());
    });
    
    LOG_INFO("Timer added, waiting...");
    
    // 等待定时器触发（使用协程sleep）
    Fiber::sleep(300);
    
    if (timer_executed.load() && counter.load() == 1) {
        LOG_INFO("PASS: Basic timer test PASSED");
    } else {
        LOG_ERROR("FAIL: Basic timer test FAILED (executed: {}, counter: {})", 
                  timer_executed.load(), counter.load());
    }
}

// 测试定时器取消
void test_timer_cancel() {
    LOG_INFO("=== Test 2: Timer Cancel ===");
    
    std::atomic<bool> should_not_execute{false};
    
    auto& timer_wheel = TimerWheel::getInstance();
    
    // 添加一个定时器并立即取消
    auto timer = timer_wheel.addTimer(static_cast<uint64_t>(100), [&]() {
        should_not_execute.store(true);
        LOG_ERROR("This timer should have been canceled!");
    });
    
    LOG_INFO("Timer added, canceling immediately...");
    timer_wheel.cancel(timer);
    
    // 等待超过定时器时间
    Fiber::sleep(200);
    
    if (!should_not_execute.load()) {
        LOG_INFO("PASS: Timer cancel test PASSED");
    } else {
        LOG_ERROR("FAIL: Timer cancel test FAILED - canceled timer executed");
    }
}

// 测试循环定时器
void test_recurring_timer() {
    LOG_INFO("=== Test 3: Recurring Timer ===");
    
    std::atomic<int> execution_count{0};
    
    auto& timer_wheel = TimerWheel::getInstance();
    
    // 添加一个每150ms触发的循环定时器
    auto timer = timer_wheel.addTimer(static_cast<uint64_t>(150), [&]() {
        int count = execution_count.fetch_add(1) + 1;
        LOG_INFO("Recurring timer execution #{}", count);
    }, true);  // repeat = true
    
    LOG_INFO("Recurring timer added, waiting for 3 executions...");
    
    // 等待大约550ms，应该执行3次
    Fiber::sleep(500);
    
    // 取消定时器
    timer_wheel.cancel(timer);
    
    int final_count = execution_count.load();
    if (final_count >= 2 && final_count <= 4) {
        LOG_INFO("PASS: Recurring timer test PASSED (executions: {})", final_count);
    } else {
        LOG_ERROR("FAIL: Recurring timer test FAILED (expected 2-4, got {})", final_count);
    }
}

// 测试协程sleep
void test_fiber_sleep() {
    LOG_INFO("=== Test 4: Fiber Sleep ===");
    
    auto start = std::chrono::steady_clock::now();
    
    LOG_INFO("Sleeping for 300ms...");
    Fiber::sleep(300);
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    LOG_INFO("Slept for {}ms", elapsed);
    
    // 允许±50ms误差
    if (elapsed >= 250 && elapsed <= 400) {
        LOG_INFO("PASS: Fiber sleep test PASSED");
    } else {
        LOG_ERROR("FAIL: Fiber sleep test FAILED (expected ~300ms, got {}ms)", elapsed);
    }
}

// 测试多个协程同时使用定时器
void test_concurrent_timers() {
    LOG_INFO("=== Test 5: Concurrent Timers ===");
    
    std::atomic<int> completed{0};
    WaitGroup wg;
    wg.add(5);
    
    for (int i = 0; i < 5; ++i) {
        Fiber::go([i, &completed, &wg]() {
            LOG_INFO("Fiber {} starting, will sleep for {}ms", i, (i+1) * 100);
            
            Fiber::sleep((i + 1) * 100);
            
            completed++;
            LOG_INFO("Fiber {} woke up! Completed count: {}", i, completed.load());
            wg.done();
        });
    }
    
    LOG_INFO("Waiting for all fibers to complete...");
    wg.wait();
    
    if (completed.load() == 5) {
        LOG_INFO("PASS: Concurrent timers test PASSED");
    } else {
        LOG_ERROR("FAIL: Concurrent timers test FAILED (completed: {})", completed.load());
    }
}

// 测试定时器刷新
void test_timer_refresh() {
    LOG_INFO("=== Test 6: Timer Refresh ===");
    
    std::atomic<int> execution_count{0};
    
    auto& timer_wheel = TimerWheel::getInstance();
    
    auto timer = timer_wheel.addTimer(static_cast<uint64_t>(200), [&]() {
        execution_count++;
        LOG_INFO("Refreshed timer executed!");
    });
    
    LOG_INFO("Timer added (200ms), refreshing after 100ms...");
    
    // 等待100ms后刷新定时器（重置超时）
    Fiber::sleep(100);
    timer = timer_wheel.refresh(timer);  // refresh返回新timer
    LOG_INFO("Timer refreshed, should trigger after another 200ms");
    
    // 再等待150ms，定时器应该还没触发
    Fiber::sleep(150);
    
    if (execution_count.load() == 0) {
        LOG_INFO("Timer not yet executed (good)");
    }
    
    // 再等待100ms，定时器应该触发了
    Fiber::sleep(100);
    
    if (execution_count.load() == 1) {
        LOG_INFO("PASS: Timer refresh test PASSED");
    } else {
        LOG_ERROR("FAIL: Timer refresh test FAILED (executions: {})", execution_count.load());
    }
}

FIBER_MAIN() {
    LOG_INFO("==================== Timer Test Started ====================");
    
    try {
        test_basic_timer();
        LOG_INFO("");
        
        test_timer_cancel();
        LOG_INFO("");
        
        test_recurring_timer();
        LOG_INFO("");
        
        test_fiber_sleep();
        LOG_INFO("");
        
        test_concurrent_timers();
        LOG_INFO("");
        
        test_timer_refresh();
        LOG_INFO("");
        
        LOG_INFO("==================== All Timer Tests Completed ====================");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        return 1;
    }
    
    return 0;
}
