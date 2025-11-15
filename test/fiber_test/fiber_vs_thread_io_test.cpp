#include "fiber.h"
#include "scheduler.h"
#include "logger.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

#include "io_fiber.h"

using namespace fiber;


constexpr int TASK_COUNT = 2000;
constexpr int DATA_SIZE = 1024; // 1KB

#include "io_manager.h"

std::atomic<int> read_count = 0;
std::atomic<int> write_count = 0;

#include <numeric>

constexpr int CPU_TASK_COUNT = 2000;
constexpr int CPU_ARRAY_SIZE = 10000;

void fiber_cpu_worker(std::atomic<int>& done) {
    std::vector<int> arr(CPU_ARRAY_SIZE, 1);
    volatile long long sum = std::accumulate(arr.begin(), arr.end(), 0LL);
    if (sum == CPU_ARRAY_SIZE) done++;
}

void thread_cpu_worker(std::atomic<int>& done) {
    std::vector<int> arr(CPU_ARRAY_SIZE, 1);
    volatile long long sum = std::accumulate(arr.begin(), arr.end(), 0LL);
    if (sum == CPU_ARRAY_SIZE) done++;
}

TEST(FiberVsThreadCPU, ComputePerformance) {
    std::atomic<int> fiber_done{0};
    auto fiber_start = std::chrono::steady_clock::now();
    for (int i = 0; i < CPU_TASK_COUNT; ++i) {
        Fiber::go([&fiber_done]() { fiber_cpu_worker(fiber_done); });
    }
    while (fiber_done.load() < CPU_TASK_COUNT) {
        Fiber::sleep(1);
    }
    auto fiber_end = std::chrono::steady_clock::now();
    auto fiber_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fiber_end - fiber_start).count();
    LOG_INFO("Fiber CPU: {} tasks, total time: {} ms", CPU_TASK_COUNT, fiber_duration);

    std::atomic<int> thread_done{0};
    auto thread_start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < CPU_TASK_COUNT; ++i) {
        threads.emplace_back([&thread_done]() { thread_cpu_worker(thread_done); });
    }
    for (auto& t : threads) t.join();
    auto thread_end = std::chrono::steady_clock::now();
    auto thread_duration = std::chrono::duration_cast<std::chrono::milliseconds>(thread_end - thread_start).count();
    LOG_INFO("Thread CPU: {} tasks, total time: {} ms", CPU_TASK_COUNT, thread_duration);

    EXPECT_LT(fiber_duration, thread_duration);
}

void fiber_reader(int fd, std::atomic<int>& done) {
    char buf[DATA_SIZE] = {0};
    LOG_INFO("fd:{} reading", fd);
    auto result = fiber::IO::read(fd, buf, DATA_SIZE); // 非阻塞协程IO
    auto rd_cnt = read_count.fetch_add(1) + 1;
    LOG_INFO("fd:{} has read, read count {}", fd, rd_cnt);
    if (result.value_or(0) == DATA_SIZE) done++;
    fiber::IO::close(fd);
}

void fiber_writer(int fd, const char* buf) {
    LOG_INFO("fd:{} writing", fd);
    fiber::IO::write(fd, buf, DATA_SIZE);
    auto wt_cnt = write_count.fetch_add(1) + 1;
    LOG_INFO("fd:{} has write, write count {}", fd - 1, wt_cnt);
    fiber::IO::close(fd);
}

void thread_reader(int fd, std::atomic<int>& done) {
    char buf[DATA_SIZE] = {0};
    ssize_t n = ::read(fd, buf, DATA_SIZE);
    if (n == DATA_SIZE) done++;
    ::close(fd);
}

void thread_writer(int fd, const char* buf) {
    ::write(fd, buf, DATA_SIZE);
    ::close(fd);
}

TEST(FiberVsThreadIO, SocketPairBlockingPerformance) {
    std::atomic<int> fiber_done{0};
    std::vector<std::pair<int,int>> fiber_fds;
    char buf[DATA_SIZE] = {0};
    // 创建socketpair
    for (int i = 0; i < TASK_COUNT; ++i) {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
            fiber_fds.emplace_back(sv[0], sv[1]);
        }
    }

    WaitGroup wg;
    wg.add(TASK_COUNT);
    // 先启动所有读worker（阻塞在read）
    for (auto& p : fiber_fds) {
        Fiber::go([fd=p.first, &fiber_done, &wg]() {
            fiber_reader(fd, fiber_done);
            // LOG_INFO("Task Doing for {}", fd);
            wg.done();
            // LOG_INFO("Task Done for {}", fd);
        });
    }
    Fiber::sleep(10); // 确保所有reader已阻塞
    auto fiber_start = std::chrono::steady_clock::now();
    // 统一写入
    LOG_INFO("// ============================= START WRITE ================================= //");

    for (auto& p : fiber_fds) {
        Fiber::go([fd=p.second, buf]() {
            fiber_writer(fd, buf);
        });
    }
    wg.wait();
    auto fiber_end = std::chrono::steady_clock::now();
    auto fiber_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fiber_end - fiber_start).count();
    LOG_INFO("Fiber socketpair blocking IO: {} tasks, total time: {} ms", TASK_COUNT, fiber_duration);

    std::atomic<int> thread_done{0};
    std::vector<std::pair<int,int>> thread_fds;
    // 创建socketpair
    for (int i = 0; i < TASK_COUNT; ++i) {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
            thread_fds.emplace_back(sv[0], sv[1]);
        }
    }
    std::vector<std::thread> threads;
    for (auto& p : thread_fds) {
        threads.emplace_back([fd=p.first, &thread_done]() { thread_reader(fd, thread_done); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto thread_start = std::chrono::steady_clock::now();
    for (auto& p : thread_fds) {
        threads.emplace_back([fd=p.second, buf]() { thread_writer(fd, buf); });
    }
    while (thread_done.load() < TASK_COUNT) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (auto& t : threads) t.join();
    auto thread_end = std::chrono::steady_clock::now();
    auto thread_duration = std::chrono::duration_cast<std::chrono::milliseconds>(thread_end - thread_start).count();
    LOG_INFO("Thread socketpair blocking IO: {} tasks, total time: {} ms", TASK_COUNT, thread_duration);

    EXPECT_LT(fiber_duration, thread_duration);
}

#include <random>
#include <future>

using namespace fiber;

// 模拟IO任务：sleep一段时间，代表一次IO
void simulated_io_task(int ms) {
    Fiber::sleep(ms);
}

void thread_io_task(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

TEST(FiberVsThreadIO, ComparePerformance) {
    const int task_count = 1000;
    const int io_ms = 10;
    const int thread_pool_size = std::thread::hardware_concurrency();

    // Fiber并发测试
    auto fiber_start = std::chrono::steady_clock::now();
    std::atomic<int> fiber_done{0};
    for (int i = 0; i < task_count; ++i) {
        Fiber::go([&fiber_done, io_ms]() {
            simulated_io_task(io_ms);
            fiber_done++;
        });
    }
    // 等待所有fiber完成
    while (fiber_done.load() < task_count) {
        Fiber::sleep(1);
    }
    auto fiber_end = std::chrono::steady_clock::now();
    auto fiber_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fiber_end - fiber_start).count();
    LOG_INFO("Fiber IO: {} tasks, total time: {} ms", task_count, (long long)fiber_duration);

    // ThreadPool并发测试
    auto thread_start = std::chrono::steady_clock::now();
    std::atomic<int> thread_done{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_pool_size; ++i) {
        threads.emplace_back([&thread_done, io_ms, task_count, thread_pool_size]() {
            for (int j = 0; j < task_count / thread_pool_size; ++j) {
                thread_io_task(io_ms);
                thread_done++;
            }
        });
    }
    for (auto& t : threads) t.join();
    auto thread_end = std::chrono::steady_clock::now();
    auto thread_duration = std::chrono::duration_cast<std::chrono::milliseconds>(thread_end - thread_start).count();
    LOG_INFO("ThreadPool IO: {} tasks, total time: {} ms", task_count, (long long)thread_duration);

    EXPECT_LT(fiber_duration, thread_duration);
}

FIBER_MAIN() {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
