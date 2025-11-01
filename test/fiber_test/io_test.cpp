#include "fiber.h"
#include "scheduler.h"
#include "io_fiber.h"
#include "sync.h"
#include "config_manager.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <chrono>

using namespace fiber;

int createListenSocket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_ERROR("socket() failed: {}", strerror(errno));
        return -1;
    }
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("bind() failed: {}", strerror(errno));
        close(sock);
        return -1;
    }
    
    if (listen(sock, 128) < 0) {
        LOG_ERROR("listen() failed: {}", strerror(errno));
        close(sock);
        return -1;
    }
    
    return sock;
}

void handleClient(int client_fd) {
    char buffer[1024];
    
    while (true) {
        auto result = IO::read(client_fd, buffer, sizeof(buffer), 5000);
        
        if (!result) {
            if (errno == ETIMEDOUT) {
                LOG_INFO("Client read timeout");
            } else {
                LOG_ERROR("Read failed: {}", strerror(errno));
            }
            break;
        }
        
        ssize_t n = *result;
        if (n == 0) {
            LOG_INFO("Client disconnected");
            break;
        }
        
        auto write_result = IO::write(client_fd, buffer, n, 5000);
        if (!write_result || *write_result != n) {
            LOG_ERROR("Write failed");
            break;
        }
    }
    
    close(client_fd);
}

void testEchoServer() {
    LOG_INFO("=== Test 1: Echo Server ===");
    
    int listen_fd = createListenSocket(9090);
    if (listen_fd < 0) {
        LOG_ERROR("FAIL: Failed to create listen socket");
        return;
    }
    
    LOG_INFO("Echo server listening on port 9090");
    
    std::atomic<int> clients_handled{0};
    
    Fiber::go([listen_fd, &clients_handled]() {
        for (int i = 0; i < 3; ++i) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            auto result = IO::accept(listen_fd, (sockaddr*)&client_addr, &addr_len, 10000);
            
            if (!result) {
                if (errno == ETIMEDOUT) {
                    LOG_INFO("Accept timeout after waiting for client");
                } else {
                    LOG_ERROR("Accept failed: {}", strerror(errno));
                }
                continue;
            }
            
            int client_fd = *result;
            
            LOG_INFO("Accepted client connection (fd={})", client_fd);
            clients_handled++;
            
            Fiber::go([client_fd]() {
                handleClient(client_fd);
            });
        }
        
        close(listen_fd);
    });
    
    Fiber::sleep(500);
    
    Fiber::go([]() {
        for (int i = 0; i < 3; ++i) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(9090);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            
            if (!IO::connect(sock, (sockaddr*)&addr, sizeof(addr), 1000)) {
                LOG_ERROR("Connect failed: {}", strerror(errno));
                close(sock);
                continue;
            }
            
            LOG_INFO("Connected to echo server");
            
            const char* msg[] = {"Hello, Fiber IO!", "Fuck you, Fiber IO", "Love you Fiber IO"};
            auto write_result = IO::write(sock, msg[i], strlen(msg[i]), 1000);
            if (!write_result) {
                LOG_ERROR("Write failed");
                close(sock);
                continue;
            }
            
            char buffer[1024];
            auto read_result = IO::read(sock, buffer, sizeof(buffer), 1000);
            if (!read_result) {
                LOG_ERROR("Read failed");
                close(sock);
                continue;
            }
            
            buffer[*read_result] = '\0';
            if (strcmp(buffer, msg[i]) == 0) {
                LOG_INFO("PASS: Echo test {}, msg: {}", i + 1, buffer);
            } else {
                LOG_ERROR("FAIL: Echo mismatch");
            }
            
            close(sock);
            Fiber::sleep(50);
        }
    });
    
    Fiber::sleep(3000);
    
    if (clients_handled.load() == 3) {
        LOG_INFO("PASS: Handled all 3 clients");
    } else {
        LOG_ERROR("FAIL: Only handled {} clients", clients_handled.load());
    }
}

void testTimeout() {
    LOG_INFO("=== Test 2: IO Timeout ===");
    
    int listen_fd = createListenSocket(9091);
    if (listen_fd < 0) {
        LOG_ERROR("FAIL: Failed to create listen socket");
        return;
    }
    
    std::atomic<bool> test_done{false};
    
    Fiber::go([listen_fd, &test_done]() {
        LOG_INFO("Starting accept with 500ms timeout...");
        auto start = std::chrono::steady_clock::now();
        auto result = IO::accept(listen_fd, nullptr, nullptr, 500);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        LOG_INFO("Accept returned after {}ms", elapsed);
        
        if (!result && errno == ETIMEDOUT && elapsed >= 400 && elapsed <= 700) {
            LOG_INFO("PASS: Accept timeout correctly ({}ms)", elapsed);
        } else {
            LOG_ERROR("FAIL: Accept timeout behavior incorrect (result={}, errno={}, elapsed={}ms)", 
                     result.has_value(), errno, elapsed);
        }
        
        close(listen_fd);
        test_done.store(true);
    });
    
    while (!test_done.load()) {
        Fiber::sleep(100);
    }
}

FIBER_MAIN() {
    LOG_INFO("================= IO Integration Test =====================");
    
    testEchoServer();
    testTimeout();
    
    LOG_INFO("==================== IO Test Completed ====================");
    return 0;
}
