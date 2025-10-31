#include "fiber.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace fiber;

void task1() {
    std::cout << "Task1: Running in background thread..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fiber::Fiber::yield();
    std::cout << "Task1: Resumed after yield" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "Task1: Completed" << std::endl;
}

void task2() {
    std::cout << "Task2: Running in background thread..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    fiber::Fiber::yield();
    std::cout << "Task2: Resumed after yield" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    std::cout << "Task2: Completed" << std::endl;
}

void task3() {
    std::cout << "Task3: Quick task in background thread" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "Task3: Completed quickly" << std::endl;
}

int main() {
    std::cout << "=== Go-style Concurrent Fiber Test ===" << std::endl;
    std::cout << "Goroutines start executing IMMEDIATELY in background threads" << std::endl;
    
    // Go语义：立即返回，协程立即在后台线程开始执行
    std::cout << "\nLaunching goroutines (they start immediately!):" << std::endl;
    
    fiber::Fiber::go(task1);
    std::cout << "- Launched task1 (already running in background!)" << std::endl;
    
    fiber::Fiber::go(task2);
    std::cout << "- Launched task2 (already running in background!)" << std::endl;
    
    fiber::Fiber::go(task3);
    std::cout << "- Launched task3 (already running in background!)" << std::endl;
    
    std::cout << "\nMain thread continues working while goroutines run..." << std::endl;
    
    // 主线程做一些工作
    for (int i = 0; i < 5; ++i) {
        std::cout << "Main thread: step " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    
    std::cout << "\nWaiting for all goroutines to complete..." << std::endl;
    fiber::Fiber::waitAll();
    
    std::cout << "=== All goroutines completed ===" << std::endl;
    
    return 0;
}