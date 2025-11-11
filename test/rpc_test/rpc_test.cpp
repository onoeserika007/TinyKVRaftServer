#include "rpc_server.h"
#include "rpc_client.h"
#include "scheduler.h"
#include "logger.h"
#include <gtest/gtest.h>
#include <memory>

using namespace rpc;

// Global server instance for all tests
static std::shared_ptr<RpcServer> g_server;

// Echo服务：简单返回接收到的消息
Json::Value echoHandler(const Json::Value& params) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string params_str = Json::writeString(writer, params);
    LOG_INFO("Echo handler called with params: {}", params_str);
    return params;  // 直接返回参数
}

// Add服务：计算两个数的和
Json::Value addHandler(const Json::Value& params) {
    int a = params["a"].asInt();
    int b = params["b"].asInt();
    int result = a + b;
    
    Json::Value response;
    response["result"] = result;
    
    LOG_INFO("Add handler: {} + {} = {}", a, b, result);
    return response;
}

// Test Echo RPC
TEST(RpcTest, EchoMethod) {
    bool test_done = false;
    bool test_passed = false;
    
    fiber::Fiber::go([&]() {
        RpcClient client;
        ASSERT_TRUE(client.connect("127.0.0.1", 9090));
        
        Json::Value params;
        params["message"] = "Hello, RPC!";
        params["id"] = 123;
        
        auto resp = client.call("echo", params, 3000);
        
        EXPECT_TRUE(resp.success);
        EXPECT_EQ(resp.result["message"].asString(), "Hello, RPC!");
        EXPECT_EQ(resp.result["id"].asInt(), 123);
        
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
        RpcClient client;
        ASSERT_TRUE(client.connect("127.0.0.1", 9090));
        
        Json::Value params;
        params["a"] = 10;
        params["b"] = 20;
        
        auto resp = client.call("add", params, 3000);
        
        EXPECT_TRUE(resp.success);
        EXPECT_EQ(resp.result["result"].asInt(), 30);
        
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
        RpcClient client;
        ASSERT_TRUE(client.connect("127.0.0.1", 9090));
        
        Json::Value params;
        auto resp = client.call("unknown_method", params, 3000);
        
        EXPECT_FALSE(resp.success);
        EXPECT_FALSE(resp.error.empty());
        
        client.disconnect();
        test_passed = true;
        test_done = true;
    });
    
    while (!test_done) {
        fiber::Fiber::sleep(10);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test concurrent RPC calls
TEST(RpcTest, ConcurrentCalls) {
    std::atomic<int> success_count{0};
    std::atomic<int> completed{0};
    const int num_clients = 5;
    const int requests_per_client = 3;
    
    for (int i = 0; i < num_clients; ++i) {
        fiber::Fiber::go([i, &success_count, &completed]() {
            RpcClient client;
            
            if (!client.connect("127.0.0.1", 9090)) {
                completed++;
                return;
            }
            
            for (int j = 0; j < requests_per_client; ++j) {
                Json::Value params;
                params["client_id"] = i;
                params["request_num"] = j;
                params["message"] = "Hello from client " + std::to_string(i);
                
                auto resp = client.call("echo", params, 3000);
                
                if (resp.success && 
                    resp.result["client_id"].asInt() == i &&
                    resp.result["request_num"].asInt() == j) {
                    success_count++;
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
    g_server = std::make_shared<RpcServer>();
    g_server->registerMethod("echo", echoHandler);
    g_server->registerMethod("add", addHandler);
    g_server->start(9090);
    
    fiber::Fiber::sleep(100);  // Wait for server to start
    
    // Run gtest
    ::testing::InitGoogleTest();
    int result = RUN_ALL_TESTS();
    
    LOG_INFO("=== RPC Tests Completed ===");
    
    fiber::Fiber::sleep(100);
    
    return result;
}
