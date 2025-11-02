#pragma once

#include "rpc_connection.h"
#include "rpc_message.h"
#include "fiber.h"
#include "logger.h"
#include <unordered_map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

namespace rpc {

// RPC方法处理器类型：接收请求参数，返回响应结果
using RpcHandler = std::function<Json::Value(const Json::Value& params)>;

class RpcServer {
public:
    RpcServer() = default;
    
    // 注册RPC方法
    void registerMethod(const std::string& method, RpcHandler handler) {
        handlers_[method] = std::move(handler);
        LOG_INFO("RpcServer: registered method '{}'", method);
    }
    
    // 启动服务器（监听端口）
    bool start(uint16_t port) {
        int listen_fd = createListenSocket(port);
        if (listen_fd < 0) {
            LOG_ERROR("RpcServer: failed to create listen socket on port {}", port);
            return false;
        }
        
        listen_fd_ = listen_fd;
        LOG_INFO("RpcServer: listening on port {}", port);
        
        // 启动accept循环
        fiber::Fiber::go([this]() {
            acceptLoop();
        });
        
        return true;
    }

private:
    // 创建监听socket
    int createListenSocket(uint16_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return -1;
        }
        
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            fiber::IO::close(sock);
            return -1;
        }
        
        if (listen(sock, 128) < 0) {
            fiber::IO::close(sock);
            return -1;
        }
        
        return sock;
    }
    
    // Accept循环
    void acceptLoop() {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            auto result = fiber::IO::accept(listen_fd_, (sockaddr*)&client_addr, &addr_len);
            
            if (!result) {
                LOG_ERROR("RpcServer: accept failed");
                continue;
            }
            
            int client_fd = *result;
            LOG_INFO("RpcServer: accepted client connection (fd={})", client_fd);
            
            // 为每个客户端启动一个fiber处理连接
            auto conn = std::make_shared<RpcConnection>(client_fd);
            fiber::Fiber::go([this, conn]() {
                handleConnection(conn);
            });
        }
    }
    
    // 处理单个连接
    void handleConnection(RpcConnectionPtr conn) {
        conn->receiveLoop([this, conn](const std::string& payload) {
            handleRequest(conn, payload);
        });
    }
    
    // 处理RPC请求
    void handleRequest(RpcConnectionPtr conn, const std::string& payload) {
        // 解析JSON
        Json::Value json;
        if (!JsonCodec::decode(payload, json)) {
            LOG_ERROR("RpcServer: failed to parse JSON");
            return;
        }
        
        // 解析请求
        RpcRequest request = RpcRequest::fromJson(json);
        
        LOG_DEBUG("RpcServer: received request id={}, method={}", 
                  request.request_id, request.method);
        
        // 查找处理器
        RpcResponse response;
        response.request_id = request.request_id;
        
        auto it = handlers_.find(request.method);
        if (it == handlers_.end()) {
            response.success = false;
            response.error = "Method not found: " + request.method;
            LOG_ERROR("RpcServer: method '{}' not found", request.method);
        } else {
            try {
                // 调用处理器
                response.result = it->second(request.params);
                response.success = true;
            } catch (const std::exception& e) {
                response.success = false;
                response.error = std::string("Exception: ") + e.what();
                LOG_ERROR("RpcServer: handler exception: {}", e.what());
            }
        }
        
        // 发送响应
        conn->sendJson(response.toJson());
    }

private:
    int listen_fd_ = -1;
    std::unordered_map<std::string, RpcHandler> handlers_;
};

} // namespace rpc
