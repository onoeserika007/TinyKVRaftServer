#include "fiber.h"
#include <iostream>
#include <vector>

using namespace fiber;

void test_function_A() {
    std::cout << "================ Test function A started ================ " << std::endl;
    Fiber::yield();
    std::cout << "================ Test function A resumed ================ " << std::endl;
}

void test_function_B() {
    std::cout << "================ Test function B started ================ " << std::endl;
    Fiber::yield();
    std::cout << "================ Test function B resumed ================ " << std::endl;
}

void test_function_C() {
    std::cout << "================ Test function C started ================ " << std::endl;
    Fiber::ptr fiber1 = Fiber::create(test_function_A);
    fiber1->resume();
    fiber1->resume();
    Fiber::yield();
    std::cout << "================ Test function C resumed ================ " << std::endl;
}

int main() {
    std::cout << "=== Fiber Test ===" << std::endl;
    
    // 创建两个协程
    Fiber::ptr fiber2 = Fiber::create(test_function_B);
    Fiber::ptr fiber3 = Fiber::create(test_function_C);
    
    std::cout << "Fiber B ID: " << fiber2->getId() << ", State: " << static_cast<int>(fiber2->getState()) << std::endl;
    std::cout << "Fiber C ID: " << fiber3->getId() << ", State: " << static_cast<int>(fiber3->getState()) << std::endl;
    
    // 执行协程
    fiber2->resume();
    fiber3->resume();

    fiber2->resume();
    fiber3->resume();
    
    std::cout << "Test completed" << std::endl;
    
    return 0;
}