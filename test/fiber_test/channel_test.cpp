#include "fiber.h"
#include "scheduler.h"
#include "channel.h"
#include "sync.h"
#include "config_manager.h"
#include "logger.h"
#include <iostream>
#include <thread>

using namespace fiber;

void producer(Channel<int>::ptr ch) {
    for (int i = 1; i <= 5; ++i) {
        if (ch->send(i)) {
            LOG_DEBUG("Producer sent: {}", i);
        } else {
            LOG_DEBUG("Producer failed to send: {}", i);
        }
    }
    
    ch->close();
    LOG_DEBUG("Producer finished and closed channel");
}

void consumer(Channel<int>::ptr ch) {
    int value;
    int sum = 0;
    while (ch->recv(value)) {
        LOG_DEBUG("Consumer received: {}", value);
        sum += value;
    }

    assert(sum == 15 && "Test 1 FAIL.");
    
    LOG_DEBUG("Consumer finished (channel closed)");
}

FIBER_MAIN() {

    LOG_INFO("================= Channel Communication Test =====================");

    // 测试1: 有缓冲Channel
    LOG_INFO("***************** Test 1: Testing buffered channel...");
    {
        auto buffered_ch = make_channel<int>(3);  // 缓冲区大小为3

        WaitGroup wg;
        wg.add(2);
        Fiber::go([buffered_ch, &wg]() {
            producer(buffered_ch);
            wg.done();
        });

        Fiber::go([buffered_ch, &wg]() {
            consumer(buffered_ch);
            wg.done();
        });

        wg.wait();

        LOG_INFO("Buffered channel test completed");
        LOG_INFO("");
    }
    
    // 等待一下让Channel完全销毁

    
    // 测试2: 无缓冲Channel
    LOG_INFO("***************** Test 2: Testing unbuffered channel...");
    {
        WaitGroup wg;
        auto unbuffered_ch = make_channel<int>(0);  // 无缓冲

        wg.add(2);
        Fiber::go([unbuffered_ch, &wg]() {
            producer(unbuffered_ch);
            wg.done();
        });

        Fiber::go([unbuffered_ch, &wg]() {
            consumer(unbuffered_ch);
            wg.done();
        });
        
        // 等待协程完成
        wg.wait();
        LOG_INFO("Unbuffered channel test completed");
    }

    LOG_INFO("==================== Channel Test Completed ====================");
    
    return 0;
}