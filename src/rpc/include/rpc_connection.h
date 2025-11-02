#pragma once

#include "buffer.h"
#include "protocol.h"
#include "rpc_message.h"
#include "serializable.h"
#include "io_fiber.h"
#include "logger.h"
#include <functional>
#include <memory>
#include <unistd.h>

namespace rpc {

// RPC连接类（管理单个TCP连接）
class RpcConnection : public std::enable_shared_from_this<RpcConnection> {
public:
    using MessageCallback = std::function<void(const std::string& payload)>;
    
    explicit RpcConnection(int fd) 
        : fd_(fd), recv_buffer_(), closed_(false) {}
    
    ~RpcConnection() {
        close();
    }
    
    // 发送消息（自动添加Length Prefix）
    bool send(const std::string& payload) {
        if (closed_) {
            return false;
        }
        
        std::string packet = Protocol::encode(payload);
        auto result = fiber::IO::write(fd_, packet.data(), packet.size());
        
        if (!result || *result != static_cast<ssize_t>(packet.size())) {
            // 连接可能已经被关闭，避免重复报错
            if (!closed_) {
                LOG_ERROR("[RpcConnection] RpcConnection send failed: fd={}", fd_);
            }
            close();
            return false;
        }
        
        return true;
    }
    
    // 发送JSON消息
    bool sendJson(const Json::Value& json) {
        std::string payload = JsonCodec::encode(json);
        return send(payload);
    }
    
    // 接收消息循环（在fiber中运行）
    void receiveLoop(MessageCallback callback) {
        char tmp[4096];
        
        while (!closed_) {
            // 读取数据
            auto result = fiber::IO::read(fd_, tmp, sizeof(tmp));
            
            if (!result) {
                break;
            }
            
            ssize_t n = *result;
            if (n == 0) {
                LOG_INFO("[RpcConnection] RpcConnection closed by peer: fd={}", fd_);
                break;
            }

            if (n < 0) {
                if (errno == EBADF) {
                    // Bad File Discriptor
                    // LOG_INFO("[RpcConnection] RpcConnection is invalid, error: {}, fd={}", strerror(errno), fd_);
                } else {
                    LOG_INFO("[RpcConnection] RpcConnection is invalid, error: {}, fd={}", strerror(errno), fd_);
                }
                break;
            }
            
            // 累积到buffer
            try {
                recv_buffer_.append(tmp, n);
            } catch (std::length_error& e) {
                LOG_ERROR("Error: {}", e.what());
            }
            
            // 尝试解析完整消息
            std::string payload;
            while (Protocol::decode(recv_buffer_, payload)) {
                // 检查连接是否已关闭
                if (closed_) {
                    break;
                }
                // 处理完整消息
                callback(payload);
            }
        }
        
        close();
    }
    
    void close() {
        if (!closed_) {
            closed_ = true;
            // 先shutdown，让正在read的fiber能够检测到连接关闭
            ::shutdown(fd_, SHUT_RDWR);
            // 使用fiber::IO::close清理IOManager状态
            fiber::IO::close(fd_);
            LOG_DEBUG("[RpcConnection] RpcConnection closed: fd={}", fd_);
        }
    }
    
    int fd() const { return fd_; }
    bool isClosed() const { return closed_; }

private:
    int fd_;
    Buffer recv_buffer_;
    bool closed_;
};

using RpcConnectionPtr = std::shared_ptr<RpcConnection>;

} // namespace rpc
