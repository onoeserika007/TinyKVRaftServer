#include "rpc_server.h"
#include "rpc_client.h"
#include "scheduler.h"
#include "logger.h"
#include <gtest/gtest.h>
#include <memory>

using namespace rpc;

// Global server instance for all tests
static std::shared_ptr<RpcServer> g_server;

// 定义请求和响应结构体
struct EchoRequest {
    std::string message;
    int id;
};

struct EchoResponse {
    std::string message;
    int id;
};

struct AddRequest {
    int a;
    int b;
};

struct AddResponse {
    int result;
};

// Echo服务：简单返回接收到的消息
std::optional<std::string> echoHandler(const EchoRequest& req, EchoResponse& resp) {
    LOG_INFO("Echo handler called with message: {}, id: {}", req.message, req.id);
    resp.message = req.message;
    resp.id = req.id;
    return std::nullopt;  // 成功
}

// Add服务：计算两个数的和
std::optional<std::string> addHandler(const AddRequest& req, AddResponse& resp) {
    resp.result = req.a + req.b;
    LOG_INFO("Add handler: {} + {} = {}", req.a, req.b, resp.result);
    return std::nullopt;  // 成功
}

// Test Echo RPC
TEST(RpcTest, EchoMethod) {
    bool test_done = false;
    bool test_passed = false;
    
    fiber::Fiber::go([&]() {
        auto client_ptr = RpcClient::Make();
        RpcClient& client = *client_ptr;
        ASSERT_TRUE(client.connect("127.0.0.1", 9090));
        
        EchoRequest req;
        req.message = "Hello, RPC!";
        req.id = 123;
        
        EchoResponse resp;
        auto error = client.call("echo", req, resp, 3000);
        
        EXPECT_FALSE(error.has_value()) << "Error: " << error.value_or("");
        EXPECT_EQ(resp.message, "Hello, RPC!");
        EXPECT_EQ(resp.id, 123);
        
        client.disconnect();
        test_passed = true;
        test_done = true;
    });
    
    while (!test_done) {
        fiber::Fiber::sleep(10);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test Add RPC
TEST(RpcTest, AddMethod) {
    bool test_done = false;
    bool test_passed = false;
    
    fiber::Fiber::go([&]() {
        auto client_ptr = RpcClient::Make();
        RpcClient& client = *client_ptr;
        ASSERT_TRUE(client.connect("127.0.0.1", 9090));
        
        AddRequest req;
        req.a = 10;
        req.b = 20;
        
        AddResponse resp;
        auto error = client.call("add", req, resp, 3000);
        
        EXPECT_FALSE(error.has_value()) << "Error: " << error.value_or("");
        EXPECT_EQ(resp.result, 30);
        
        client.disconnect();
        test_passed = true;
        test_done = true;
    });
    
    while (!test_done) {
        fiber::Fiber::sleep(10);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test unknown method
TEST(RpcTest, UnknownMethod) {
    bool test_done = false;
    bool test_passed = false;
    
    fiber::Fiber::go([&]() {
        auto client_ptr = RpcClient::Make();
        RpcClient& client = *client_ptr;
        ASSERT_TRUE(client.connect("127.0.0.1", 9090));
        
        EchoRequest req;
        req.message = "test";
        req.id = 1;
        
        EchoResponse resp;
        auto error = client.call("unknown_method", req, resp, 3000);
        
        EXPECT_TRUE(error.has_value());
        EXPECT_FALSE(error.value().empty());
        
        client.disconnect();
        test_passed = true;
        test_done = true;
    });
    
    while (!test_done) {
        fiber::Fiber::sleep(10);
    }
    
    EXPECT_TRUE(test_passed);
}

// Request/Response for concurrent test
struct ConcurrentRequest {
    int client_id;
    int request_num;
    std::string message;
};

struct ConcurrentResponse {
    int client_id;
    int request_num;
    std::string message;
};

// Handler for concurrent test
std::optional<std::string> concurrentHandler(const ConcurrentRequest& req, ConcurrentResponse& resp) {
    resp.client_id = req.client_id;
    resp.request_num = req.request_num;
    resp.message = req.message;
    return std::nullopt;
}

// Test concurrent RPC calls
TEST(RpcTest, ConcurrentCalls) {
    // Register concurrent handler
    g_server->registerHandler("concurrent", concurrentHandler);
    fiber::Fiber::sleep(50);
    
    std::atomic<int> success_count{0};
    std::atomic<int> completed{0};
    const int num_clients = 5;
    const int requests_per_client = 3;

    fiber::WaitGroup wg;
    wg.add(num_clients * requests_per_client);
    
    for (int i = 0; i < num_clients; ++i) {
        fiber::Fiber::go([i, &success_count, &completed, &wg]() {
            auto client_ptr = RpcClient::Make();
            RpcClient& client = *client_ptr;
            
            if (!client.connect("127.0.0.1", 9090)) {
                completed++;
                return;
            }
            
            for (int j = 0; j < requests_per_client; ++j) {
                ConcurrentRequest req;
                req.client_id = i;
                req.request_num = j;
                req.message = "Hello from client " + std::to_string(i);
                
                ConcurrentResponse resp;
                auto error = client.call("concurrent", req, resp, 3000);
                
                if (!error.has_value() && 
                    resp.client_id == i &&
                    resp.request_num == j) {
                    success_count++;
                } else {
                    LOG_ERROR("Error Msg: {}", error.value());
                }
                
                fiber::Fiber::sleep(50);
            }
            
            client.disconnect();
            completed++;
        });
        
        fiber::Fiber::sleep(10);
    }
    
    // Wait for all clients to complete
    while (completed < num_clients) {
        fiber::Fiber::sleep(100);
    }
    
    EXPECT_EQ(success_count, num_clients * requests_per_client);
}

FIBER_MAIN() {
    LOG_INFO("=== Starting RPC Tests ===");
    
    // Start RPC server
    g_server = RpcServer::Make();
    g_server->registerHandler("echo", echoHandler);
    g_server->registerHandler("add", addHandler);
    g_server->start(9090);
    
    fiber::Fiber::sleep(100);  // Wait for server to start
    
    // Run gtest
    ::testing::InitGoogleTest();
    int result = RUN_ALL_TESTS();
    
    LOG_INFO("=== RPC Tests Completed ===");
    
    fiber::Fiber::sleep(100);

    g_server = nullptr;
    
    return result;
}
