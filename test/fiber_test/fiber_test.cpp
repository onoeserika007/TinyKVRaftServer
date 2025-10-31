#include "fiber.h"
#include <iostream>
#include <vector>

using namespace fiber;

void test_function_A() {
    std::cout << "================ Test function A started ================ " << std::endl;
    Fiber::YieldCurrent();
    std::cout << "================ Test function A resumed ================ " << std::endl;
}

void test_function_B() {
    std::cout << "================ Test function B started ================ " << std::endl;
    Fiber::YieldCurrent();
    std::cout << "================ Test function B resumed ================ " << std::endl;
}

int main() {
    std::cout << "=== Fiber Test ===" << std::endl;
    
    // 获取主协程
    Fiber::ptr main_fiber = Fiber::GetCurrentFiber();
    std::cout << "Main fiber ID: " << main_fiber->getId() << std::endl;
    
    // 创建两个协程
    Fiber::ptr fiber2 = std::make_shared<Fiber>(test_function_A);
    Fiber::ptr fiber3 = std::make_shared<Fiber>(test_function_B);
    
    std::cout << "Fiber A ID: " << fiber2->getId() << ", State: " << static_cast<int>(fiber2->getState()) << std::endl;
    std::cout << "Fiber B ID: " << fiber3->getId() << ", State: " << static_cast<int>(fiber3->getState()) << std::endl;
    
    // 执行协程
    fiber2->resume();
    fiber3->resume();

    fiber2->resume();
    fiber3->resume();
    
    std::cout << "Test completed" << std::endl;
    
    return 0;
}