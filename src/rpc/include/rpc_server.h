#pragma once

#include "rpc_connection.h"
#include "rpc_message.h"
#include "server_config.h"
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
    RpcServer() : running_(false), listen_fd_(-1) {}
    
    ~RpcServer() {
        shutdown();
    }
    
    // 注册RPC方法
    void registerMethod(const std::string& method, RpcHandler handler) {
        handlers_[method] = std::move(handler);
        LOG_INFO("RpcServer: registered method '{}'", method);
    }
    
    // 监听端口 - 测试
    bool start(uint16_t port) {
        ServerConfig config(port);
        config.listen_addr = "127.0.0.1";
        return start(config);
    }
    
    // 完整配置 - 生产
    bool start(const ServerConfig& config) {
        if (running_) {
            LOG_WARN("RpcServer: already running");
            return false;
        }
        
        // 保存配置
        config_ = config;
        
        // 创建监听socket
        int listen_fd = createListenSocket(config.port);
        if (listen_fd < 0) {
            LOG_ERROR("RpcServer: failed to create listen socket on port {}", config.port);
            return false;
        }
        
        listen_fd_ = listen_fd;
        port_ = config.port;
        running_ = true;
        LOG_INFO("RpcServer: listening on {} port {}", config.listen_addr, port_);
        
        // TODO: 如果配置了服务注册，在这里注册到注册中心
        // if (config_.registry_type != RegistryType::NONE) {
        //     registerToRegistry();
        // }
        
        // 启动accept循环
        fiber::Fiber::go([this]() {
            acceptLoop();
        });
        
        return true;
    }
    
    // 获取实际监听的端口（如果配置为0则自动分配）
    uint16_t getActualPort() const {
        return port_;
    }
    
    // 获取服务器配置
    const ServerConfig& getConfig() const {
        return config_;
    }
    
    // 关闭服务器
    void shutdown() {
        if (!running_) {
            return;
        }
        
        LOG_INFO("RpcServer: shutting down (port {})", port_);
        running_ = false;
        
        // 关闭监听socket以中断acceptLoop
        if (listen_fd_ >= 0) {
            fiber::IO::close(listen_fd_);
            listen_fd_ = -1;
        }
        
        // 清理处理器
        handlers_.clear();
    }
    
    bool isRunning() const {
        return running_;
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
        while (running_) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            auto result = fiber::IO::accept(listen_fd_, (sockaddr*)&client_addr, &addr_len);
            
            if (!result) {
                // accept失败，可能是因为shutdown关闭了socket
                if (!running_) {
                    LOG_INFO("RpcServer: accept loop terminated");
                    break;
                }
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
        LOG_INFO("RpcServer: accept loop exited");
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
    bool running_;
    int listen_fd_;
    uint16_t port_;
    ServerConfig config_;  // 服务器配置（新增，用于生产模式）
    std::unordered_map<std::string, RpcHandler> handlers_;
};

} // namespace rpc
