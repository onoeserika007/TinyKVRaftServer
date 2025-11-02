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

// 测试Channel超时功能
void test_channel_timeout() {
    LOG_INFO("***************** Test 3: Testing channel timeout operations...");
    
    // 测试1: send_timeout - 超时场景
    LOG_INFO("Test 3.1: send_timeout on full channel (should timeout)");
    {
        auto ch = make_channel<int>(1);  // 容量为1
        ch->send(100);  // 填满channel
        
        auto start = std::chrono::steady_clock::now();
        bool result = ch->send_timeout(200, 150);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (!result && elapsed >= 100 && elapsed <= 250) {
            LOG_INFO("PASS: send_timeout timed out correctly ({}ms)", elapsed);
        } else {
            LOG_ERROR("FAIL: send_timeout behavior incorrect (result={}, elapsed={}ms)", result, elapsed);
        }
    }
    
    // 测试2: send_timeout - 成功发送场景
    LOG_INFO("Test 3.2: send_timeout with consumer (should succeed)");
    {
        auto ch = make_channel<int>(0);  // 无缓冲
        std::atomic<bool> sent{false};
        
        Fiber::go([ch, &sent]() {
            auto start = std::chrono::steady_clock::now();
            bool result = ch->send_timeout(42, 500);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            
            if (result && elapsed < 400) {
                LOG_INFO("PASS: send_timeout succeeded ({}ms)", elapsed);
                sent = true;
            } else {
                LOG_ERROR("FAIL: send_timeout should succeed (result={}, elapsed={}ms)", result, elapsed);
            }
        });
        
        Fiber::sleep(200);  // 等待200ms后接收
        int value;
        ch->recv(value);
        
        Fiber::sleep(200);
        if (value == 42 && sent) {
            LOG_INFO("PASS: Value received correctly");
        } else {
            LOG_ERROR("FAIL: Value mismatch or send failed");
        }
    }
    
    // 测试3: recv_timeout - 超时场景
    LOG_INFO("Test 3.3: recv_timeout on empty channel (should timeout)");
    {
        auto ch = make_channel<int>(1);
        int value = -1;
        
        auto start = std::chrono::steady_clock::now();
        bool result = ch->recv_timeout(value, 150);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (!result && elapsed >= 100 && elapsed <= 250 && value == -1) {
            LOG_INFO("PASS: recv_timeout timed out correctly ({}ms)", elapsed);
        } else {
            LOG_ERROR("FAIL: recv_timeout behavior incorrect (result={}, elapsed={}ms, value={})", 
                     result, elapsed, value);
        }
    }
    
    // 测试4: recv_timeout - 成功接收场景
    LOG_INFO("Test 3.4: recv_timeout with producer (should succeed)");
    {
        auto ch = make_channel<int>(0);
        std::atomic<int> received{0};
        
        Fiber::go([ch, &received]() {
            int value = -1;
            auto start = std::chrono::steady_clock::now();
            bool result = ch->recv_timeout(value, 500);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            
            if (result && elapsed < 400 && value == 88) {
                LOG_INFO("PASS: recv_timeout succeeded ({}ms, value={})", elapsed, value);
                received = value;
            } else {
                LOG_ERROR("FAIL: recv_timeout should succeed (result={}, elapsed={}ms, value={})", 
                         result, elapsed, value);
            }
        });
        
        Fiber::sleep(200);  // 等待200ms后发送
        ch->send(88);
        
        Fiber::sleep(200);
        if (received == 88) {
            LOG_INFO("PASS: Value matched correctly");
        } else {
            LOG_ERROR("FAIL: Value mismatch: {}", received.load());
        }
    }
    
    LOG_INFO("Channel timeout test completed");
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

    test_channel_timeout();

    LOG_INFO("==================== Channel Test Completed ====================");
    
    return 0;
}