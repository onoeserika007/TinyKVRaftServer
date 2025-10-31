#include "scheduler.h"
#include "fiber.h"
#include <iostream>
#include <vector>

using namespace fiber;

void scheduler_test_function_1() {
    std::cout << "================ Test Function 1 Started ================ " << std::endl;
    Fiber::YieldToScheduler();
    std::cout << "================ Test Function 1 Resumed ================ " << std::endl;
}

void scheduler_test_function_2() {
    std::cout << "================ Test Function 2 Started ================ " << std::endl;
    Fiber::YieldToScheduler();
    std::cout << "================ Test Function 2 Resumed ================ " << std::endl;
}

void scheduler_test_function_3() {
    std::cout << "================ Test Function 3 Started ================ " << std::endl;
    Fiber::YieldToScheduler();
    std::cout << "================ Test Function 3 Resumed ================ " << std::endl;
}

int main() {
    std::cout << "=== Scheduler Test ===" << std::endl;
    
    // 创建调度器
    Scheduler::ptr scheduler = std::make_shared<Scheduler>();
    Scheduler::SetScheduler(scheduler);
    
    std::cout << "Scheduler created" << std::endl;
    
    // 初始化调度器
    scheduler->init();
    
    // 添加协程到调度器
    auto fiber1 = std::make_shared<Fiber>(scheduler_test_function_1);
    auto fiber2 = std::make_shared<Fiber>(scheduler_test_function_2);
    auto fiber3 = std::make_shared<Fiber>(scheduler_test_function_3);
    
    scheduler->schedule(fiber1);
    scheduler->schedule(fiber2);
    scheduler->schedule(fiber3);
    
    std::cout << "Scheduled 3 fibers" << std::endl;
    std::cout << "Ready fibers: " << (scheduler->hasReadyFibers() ? "Yes" : "No") << std::endl;
    
    // 启动调度器
    std::cout << "Starting scheduler execution..." << std::endl;
    scheduler->start();
    
    // 停止调度器
    scheduler->stop();
    
    std::cout << "Scheduler test completed" << std::endl;
    
    return 0;
}