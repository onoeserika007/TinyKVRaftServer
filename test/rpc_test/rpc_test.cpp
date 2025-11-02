#include "rpc_server.h"
#include "rpc_client.h"
#include "scheduler.h"
#include "logger.h"
#include <iostream>

using namespace rpc;

// Echo服务：简单返回接收到的消息
Json::Value echoHandler(const Json::Value& params) {
    LOG_INFO("Echo handler called with params: {}", JsonCodec::encode(params));
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

void testEchoRpc() {
    LOG_INFO("=== Test 1: Echo RPC ===");
    
    // 启动客户端
    fiber::Fiber::go([]() {
        RpcClient client;
        
        if (!client.connect("127.0.0.1", 9090)) {
            LOG_ERROR("Failed to connect to server");
            return;
        }
        
        // 测试Echo
        {
            LOG_INFO("****** START: Echo test *********");
            Json::Value params;
            params["message"] = "Hello, RPC!";
            params["id"] = 123;
            
            auto resp = client.call("echo", params, 3000);
            
            if (resp.success) {
                LOG_INFO("✓ Echo test PASS: {}", JsonCodec::encode(resp.result));
            } else {
                LOG_ERROR("✗ Echo test FAIL: {}", resp.error);
            }
        }
        
        // 测试Add
        {
            LOG_INFO("****** START: Add test *********");
            Json::Value params;
            params["a"] = 10;
            params["b"] = 20;
            
            auto resp = client.call("add", params, 3000);
            
            if (resp.success && resp.result["result"].asInt() == 30) {
                LOG_INFO("✓ Add test PASS: 10 + 20 = {}", resp.result["result"].asInt());
            } else {
                LOG_ERROR("✗ Add test FAIL");
            }
        }
        
        // 测试不存在的方法
        {
            LOG_INFO("****** START: Unknown method test *********");
            Json::Value params;
            auto resp = client.call("unknown_method", params, 3000);
            
            if (!resp.success) {
                LOG_INFO("✓ Unknown method test PASS: {}", resp.error);
            } else {
                LOG_ERROR("✗ Unknown method test FAIL: should return error");
            }
        }
        
        client.disconnect();
    });
    
    fiber::Fiber::sleep(2000);
}

void testConcurrentRpc() {
    LOG_INFO("=== Test 2: Concurrent RPC ===");
    
    // 服务器已经在运行
    
    // 启动多个客户端并发调用
    for (int i = 0; i < 5; ++i) {
        fiber::Fiber::go([i]() {
            RpcClient client;
            
            if (!client.connect("127.0.0.1", 9090)) {
                LOG_ERROR("Client {} failed to connect", i);
                return;
            }
            
            for (int j = 0; j < 3; ++j) {
                Json::Value params;
                params["client_id"] = i;
                params["request_num"] = j;
                params["message"] = "Hello from client " + std::to_string(i);
                
                auto resp = client.call("echo", params, 3000);
                
                if (resp.success) {
                    LOG_INFO("Client {} request {} PASS", i, j);
                } else {
                    LOG_ERROR("Client {} request {} FAIL: {}", i, j, resp.error);
                }
                
                fiber::Fiber::sleep(50);
            }
            
            client.disconnect();
        });
        
        fiber::Fiber::sleep(10);  // 错开连接时间
    }
    
    fiber::Fiber::sleep(3000);
}

FIBER_MAIN() {
    LOG_INFO("================= RPC Test Starting =====================");
    // 启动服务器
    RpcServer server;
    server.registerMethod("echo", echoHandler);
    server.registerMethod("add", addHandler);
    server.start(9090);

    fiber::Fiber::sleep(100);  // 等待服务器启动

    testEchoRpc();
    testConcurrentRpc();
    
    LOG_INFO("==================== RPC Test Completed ====================");
    
    fiber::Fiber::sleep(500);
    
    return 0;
}
