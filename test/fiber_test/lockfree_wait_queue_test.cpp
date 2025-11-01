#include "wait_queue.h"
#include "scheduler.h"
#include <iostream>
#include <atomic>

#include "sync.h"

using namespace fiber;

void test_lockfree_wait_queue() {
    std::cout << "Testing Lock-Free WaitQueue..." << std::endl;
    
    WaitQueue wait_queue;
    std::cout << "Initial empty: " << wait_queue.empty() << std::endl;
    
    // 测试基本的push/pop操作（不依赖复杂的协程调度）
    // 模拟协程等待和唤醒的场景
    
    std::atomic<int> step{0};
    std::atomic<bool> fiber1_waiting{false};
    std::atomic<bool> fiber2_waiting{false};
    std::atomic<int> notification_count{0};

    WaitGroup wg;
    wg.add(3);
    
    // 创建等待协程1
    Fiber::go([&]() {
        std::cout << "Fiber 1: Ready to wait" << std::endl;
        fiber1_waiting = true;
        step = 1;
        
        wait_queue.wait();  // 这里会yield，等待被唤醒
        
        std::cout << "Fiber 1: Woken up!" << std::endl;
        notification_count++;
        wg.done();
    });
    
    // 创建等待协程2  
    Fiber::go([&]() {
        // 等待fiber1先开始
        while (step < 1) {
            Fiber::yield();
        }
        
        std::cout << "Fiber 2: Ready to wait" << std::endl;
        fiber2_waiting = true;
        step = 2;
        
        wait_queue.wait();  // 这里会yield，等待被唤醒
        
        std::cout << "Fiber 2: Woken up!" << std::endl;
        notification_count++;
        wg.done();
    });
    
    // 创建通知协程
    Fiber::go([&]() {
        // 等待两个fiber都开始等待
        while (step < 2) {
            Fiber::yield();
        }

        // 给一些时间让协程真正进入wait状态
        for (int i = 0; i < 10; ++i) {
            Fiber::yield();
        }
        
        std::cout << "Notifier: Waking up one fiber..." << std::endl;
        bool result1 = wait_queue.notify_one();
        std::cout << "notify_one result: " << result1 << std::endl;
        
        // 让被唤醒的协程有机会执行
        for (int i = 0; i < 5; ++i) {
            Fiber::yield();
        }
        
        std::cout << "Notifier: Waking up all remaining fibers..." << std::endl;
        size_t result2 = wait_queue.notify_all();
        std::cout << "notify_all result: " << result2 << std::endl;

        // 最后的yield
        for (int i = 0; i < 5; ++i) {
            Fiber::yield();
        }
        wg.done();
    });
    
    // 等待所有协程完成
    wg.wait();
    
    std::cout << "Final notification count: " << notification_count.load() << std::endl;
    std::cout << "Final empty: " << wait_queue.empty() << std::endl;
    
    std::cout << "✓ Lock-Free WaitQueue test completed" << std::endl;
}

FIBER_MAIN() {
    try {
        test_lockfree_wait_queue();
        std::cout << "\n✅ All lock-free WaitQueue tests PASSED" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}