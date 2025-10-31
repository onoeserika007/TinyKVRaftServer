#include "fiber.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace fiber;

void fastTask(int id) {
    std::cout << "Fast task " << id << " starting (no yield)" << std::endl;
    
    // 模拟一些快速工作
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "Fast task " << id << " completed" << std::endl;
}

void slowTask(int id) {
    std::cout << "Slow task " << id << " starting" << std::endl;
    
    // 模拟一些工作
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::cout << "Slow task " << id << " yielding..." << std::endl;
    fiber::Fiber::yield();
    
    // 继续工作
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    std::cout << "Slow task " << id << " completed" << std::endl;
}

void printTask(const std::string& message) {
    for (int i = 0; i < 5; ++i) {
        std::cout << message << " - step " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (i == 2) {
            std::cout << message << " yielding at step " << i << std::endl;
            fiber::Fiber::yield();
        }
    }
    std::cout << message << " completed all steps" << std::endl;
}

int main() {
    std::cout << "=== True Go-style Concurrent Fiber Test ===" << std::endl;
    std::cout << "Goroutines start executing IMMEDIATELY in background threads" << std::endl;
    
    // 现在默认就是并发模式
    std::cout << "\nConcurrent mode is now the default!" << std::endl;
    
    std::cout << "\nLaunching goroutines (they start executing immediately!):" << std::endl;
    
    // 启动多个goroutine - 它们应该立即开始执行！
    fiber::Fiber::go([](){ slowTask(1); });
    std::cout << "- Launched slow task 1 (already running in background!)" << std::endl;

    fiber::Fiber::go([](){ fastTask(1); });
    std::cout << "- Launched fast task 1 (already running in background!)" << std::endl;
    
    fiber::Fiber::go([](){ printTask("TaskA"); });
    std::cout << "- Launched print task A (already running in background!)" << std::endl;
    
    fiber::Fiber::go([](){ fastTask(2); });
    std::cout << "- Launched fast task 2 (already running in background!)" << std::endl;
    
    fiber::Fiber::go([](){ printTask("TaskB"); });
    std::cout << "- Launched print task B (already running in background!)" << std::endl;
    
    // 主线程继续执行其他工作
    std::cout << "\nMain thread doing other work while goroutines run in background..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        std::cout << "Main thread work step " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\nMain thread finished its work. Goroutines may still be running..." << std::endl;
    
    // 给goroutines一些时间完成
    std::cout << "Waiting for goroutines to complete..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "\n=== Test completed ===" << std::endl;
    // 注意：调度器和FiberConsumer会在程序结束时自动清理
    
    return 0;
}