#include "fiber.h"
#include <iostream>

using namespace fiber;

void simpleTask1() {
    std::cout << "================ Simple Task 1 Started ================" << std::endl;
    
    // 让出执行权
    Fiber::yield();
    
    std::cout << "================ Simple Task 1 Resumed ================" << std::endl;
}

void simpleTask2() {
    std::cout << "================ Simple Task 2 Started ================" << std::endl;
    
    // 让出执行权  
    Fiber::yield();
    
    std::cout << "================ Simple Task 2 Resumed ================" << std::endl;
}

void simpleTask3() {
    std::cout << "================ Simple Task 3 Started ================" << std::endl;
    
    // 这个任务不让出执行权，直接完成
    
    std::cout << "================ Simple Task 3 Completed ================" << std::endl;
}

int main() {
    std::cout << "=== User-Friendly Fiber Interface Test ===" << std::endl;
    
    // 用户友好接口：调度器对用户完全透明
    // 用户只需要调用 CreateAndStart 即可
    
    std::cout << "Creating fibers with transparent scheduling..." << std::endl;
    
    // 使用新的Go语义接口，协程立即开始执行
    std::cout << "Creating and running fiber 1..." << std::endl;
    Fiber::go(simpleTask1);
    
    std::cout << "Creating and running fiber 2..." << std::endl;
    Fiber::go(simpleTask2);  
    
    std::cout << "Creating and running fiber 3..." << std::endl;
    Fiber::go(simpleTask3);
    
    std::cout << "All fibers completed automatically!" << std::endl;
    
    return 0;
}