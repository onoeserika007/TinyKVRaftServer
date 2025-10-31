#include "fiber.h"
#include <iostream>

using namespace fiber;

void task1() {
    std::cout << "Task1: Starting..." << std::endl;
    fiber::Fiber::yield();
    std::cout << "Task1: After first yield" << std::endl;
    fiber::Fiber::yield();
    std::cout << "Task1: Completed" << std::endl;
}

void task2() {
    std::cout << "Task2: Starting..." << std::endl;
    fiber::Fiber::yield();
    std::cout << "Task2: After first yield" << std::endl;
    fiber::Fiber::yield();
    std::cout << "Task2: Completed" << std::endl;
}

int main() {
    std::cout << "=== Lua-style Fiber Test ===" << std::endl;
    std::cout << "User has FULL control over execution order" << std::endl;
    
    // 创建fiber（不会立即执行）
    auto fiber1 = fiber::Fiber::create(task1);
    auto fiber2 = fiber::Fiber::create(task2);
    
    std::cout << "\nFibers created, now user controls execution:" << std::endl;
    
    // 用户完全控制执行顺序
    std::cout << "\n1. Resume fiber1 first time:" << std::endl;
    fiber1->resume();  // task1: Starting... -> yield
    
    std::cout << "\n2. Resume fiber2 first time:" << std::endl;
    fiber2->resume();  // task2: Starting... -> yield
    
    std::cout << "\n3. Resume fiber1 second time:" << std::endl;
    fiber1->resume();  // task1: After first yield -> yield
    
    std::cout << "\n4. Resume fiber2 second time:" << std::endl;
    fiber2->resume();  // task2: After first yield -> yield
    
    std::cout << "\n5. Finish fiber1:" << std::endl;
    fiber1->resume();  // task1: Completed
    
    std::cout << "\n6. Finish fiber2:" << std::endl;
    fiber2->resume();  // task2: Completed
    
    std::cout << "\n=== All fibers completed with user control ===" << std::endl;
    
    return 0;
}