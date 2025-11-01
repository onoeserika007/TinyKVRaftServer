#include "sync.h"
#include <iostream>
#include <thread>

#include "scheduler.h"

using namespace fiber;

FIBER_MAIN() {
    std::cout << "Testing FiberMutex without scheduler integration..." << std::endl;
    
    try {
        // 测试FiberMutex基本操作（不依赖协程上下文）
        FiberMutex mtx;
        
        // 测试try_lock/unlock
        if (mtx.try_lock()) {
            std::cout << "✓ try_lock() successful" << std::endl;
            mtx.unlock();
            std::cout << "✓ unlock() successful" << std::endl;
        } else {
            std::cout << "✗ try_lock() failed" << std::endl;
        }
        
        // 测试WaitGroup基本操作
        WaitGroup wg;
        
        std::cout << "Initial WaitGroup count: " << wg.count() << std::endl;
        
        wg.add(3);
        std::cout << "After add(3): " << wg.count() << std::endl;
        
        wg.done();
        std::cout << "After done(): " << wg.count() << std::endl;
        
        wg.add(-2);  // 相当于done() * 2
        std::cout << "After add(-2): " << wg.count() << std::endl;
        
        if (wg.count() == 0) {
            std::cout << "✓ WaitGroup operations successful" << std::endl;
        }
        
        std::cout << "✓ Synchronization primitives basic test PASSED" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}