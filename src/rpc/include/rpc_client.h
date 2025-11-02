#pragma once

#include "rpc_connection.h"
#include "rpc_message.h"
#include "fiber.h"
#include "sync.h"
#include "channel.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace rpc {

// RPC客户端
class RpcClient {
public:
    RpcClient() : next_request_id_(1), connected_(false) {}
    
    ~RpcClient() {
        disconnect();
    }
    
    // 连接到RPC服务器
    bool connect(const std::string& host, uint16_t port, int64_t timeout_ms = 3000) {
        if (connected_) {
            LOG_WARN("RpcClient: already connected");
            return true;
        }
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            LOG_ERROR("RpcClient: socket() failed");
            return false;
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        
        if (!fiber::IO::connect(sock, (sockaddr*)&addr, sizeof(addr), timeout_ms)) {
            LOG_ERROR("RpcClient: connect to {}:{} failed", host, port);
            fiber::IO::close(sock);
            return false;
        }
        
        conn_ = std::make_shared<RpcConnection>(sock);
        connected_ = true;
        
        LOG_INFO("RpcClient: connected to {}:{}", host, port);
        
        // 启动接收循环
        fiber::Fiber::go([this]() {
            receiveLoop();
        });
        
        return true;
    }
    
    // 断开连接
    void disconnect() {
        if (!connected_) {
            return;
        }
        
        connected_ = false;
        
        auto conn_fd = -1;
        if (conn_) {
            conn_fd = conn_->fd();
            conn_->close();  // 这会触发receiveLoop退出
            conn_.reset();   // 释放连接对象
        }
        
        // 清理所有pending的请求
        {
            std::lock_guard<fiber::FiberMutex> lock(pending_mutex_);
            pending_requests_.clear();
        }
        
        LOG_INFO("RpcClient: disconnected fd:{}", conn_fd);
    }
    
    // 同步RPC调用
    RpcResponse call(const std::string& method, const Json::Value& params, int64_t timeout_ms = 5000) {
        if (!connected_) {
            RpcResponse resp;
            resp.success = false;
            resp.error = "Not connected";
            return resp;
        }
        
        // 构造请求
        RpcRequest request;
        request.request_id = next_request_id_.fetch_add(1);
        request.method = method;
        request.params = params;
        
        // 创建等待响应的Channel
        auto response_chan = fiber::make_channel<RpcResponse>(1);
        {
            std::lock_guard<fiber::FiberMutex> lock(pending_mutex_);
            pending_requests_[request.request_id] = response_chan;
        }
        
        // 发送请求
        if (!conn_->sendJson(request.toJson())) {
            std::lock_guard<fiber::FiberMutex> lock(pending_mutex_);
            pending_requests_.erase(request.request_id);
            
            RpcResponse resp;
            resp.request_id = request.request_id;
            resp.success = false;
            resp.error = "Send failed";
            return resp;
        }
        
        LOG_DEBUG("RpcClient: sent request id={}, method={}", request.request_id, method);
        
        // 等待响应（带超时）
        RpcResponse response;
        if (response_chan->recv_timeout(response, timeout_ms)) {
            return response;
        } else {
            // 超时
            std::lock_guard<fiber::FiberMutex> lock(pending_mutex_);
            pending_requests_.erase(request.request_id);
            
            response.request_id = request.request_id;
            response.success = false;
            response.error = "Request timeout";
            return response;
        }
    }

private:
    // 接收循环
    void receiveLoop() {
        conn_->receiveLoop([this](const std::string& payload) {
            handleResponse(payload);
        });
    }
    
    // 处理响应
    void handleResponse(const std::string& payload) {
        // 解析JSON
        Json::Value json;
        if (!JsonCodec::decode(payload, json)) {
            LOG_ERROR("RpcClient: failed to parse JSON response");
            return;
        }
        
        // 解析响应
        RpcResponse response = RpcResponse::fromJson(json);
        
        LOG_DEBUG("RpcClient: received response id={}, fd={}", response.request_id, conn_->fd());
        
        // 查找等待的请求
        std::shared_ptr<fiber::Channel<RpcResponse>> chan;
        {
            std::lock_guard<fiber::FiberMutex> lock(pending_mutex_);
            auto it = pending_requests_.find(response.request_id);
            if (it != pending_requests_.end()) {
                chan = it->second;
                pending_requests_.erase(it);
            }
        }
        
        // 唤醒等待的fiber
        if (chan) {
            chan->send(response);
        } else {
            LOG_WARN("RpcClient: received response for unknown request id={}", 
                     response.request_id);
        }
    }

private:
    RpcConnectionPtr conn_;
    std::atomic<uint64_t> next_request_id_;
    bool connected_;
    
    fiber::FiberMutex pending_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<fiber::Channel<RpcResponse>>> pending_requests_;
};

} // namespace rpc
