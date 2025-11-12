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
#include <optional>
#include "encoder.h"
#include "rpc_serializer_pfr.h"

namespace rpc {

// 前向声明 Encoder/Decoder
class Encoder;
class Decoder;
class RpcServer;

// RPC方法处理器类型：接收序列化的字符串参数，返回序列化的字符串结果
// 处理器内部负责反序列化输入、调用业务逻辑、序列化输出
using RpcHandler = std::function<std::string(const std::string& params_data)>;
using RpcServerPtr = std::shared_ptr<RpcServer>;

class RpcServer: public std::enable_shared_from_this<RpcServer> {
public:

    static RpcServerPtr Make() {
        return std::shared_ptr<RpcServer>(new RpcServer);
    }
    
    ~RpcServer() {
        shutdown();
    }
    
    // 模板化的类型安全注册方法 - 主要接口
    // Func: std::optional<std::string> func(const InputArgs&, OutputArgs&)
    // 返回值：std::nullopt = 成功，有值 = 错误消息
    template<typename InputArgs, typename OutputArgs>
    void registerHandler(const std::string& method, 
                        std::optional<std::string> (*func)(const InputArgs&, OutputArgs&)) {
        registerMethod(method, [func](const std::string& params_data) -> std::string {
            // 反序列化输入
            auto decoder = Decoder::New(params_data);
            InputArgs input;
            if (!decoder->Decode(input)) {
                throw std::runtime_error("Failed to decode input arguments");
            }
            
            // 调用业务逻辑
            OutputArgs output;
            auto error = func(input, output);
            if (error.has_value()) {
                throw std::runtime_error(error.value());
            }
            
            // 序列化输出
            auto encoder = Encoder::New();
            encoder->Encode(output);
            return encoder->Bytes();
        });
    }
    
    // Lambda 版本（用于捕获上下文）
    template<typename Func>
    void registerHandler(const std::string& method, Func func) {
        using InputArgs = std::decay_t<std::remove_reference_t<
            std::tuple_element_t<0, typename function_traits<Func>::args>>>;
        using OutputArgs = std::decay_t<std::remove_reference_t<
            std::tuple_element_t<1, typename function_traits<Func>::args>>>;
        
        registerMethod(method, [func](const std::string& params_data) -> std::string {
            // 反序列化输入
            auto decoder = Decoder::New(params_data);
            InputArgs input;
            if (!decoder->Decode(input)) {
                throw std::runtime_error("Failed to decode input arguments");
            }
            
            // 调用业务逻辑
            OutputArgs output;
            auto error = func(input, output);
            if (error.has_value()) {
                throw std::runtime_error(error.value());
            }
            
            // 序列化输出
            auto encoder = Encoder::New();
            encoder->Encode(output);
            return encoder->Bytes();
        });
    }

public:
    
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

        try {
            // 启动accept循环RpcServer: shutting down
            fiber::Fiber::go([server = shared_from_this()]() {
                server->acceptLoop();
            });
        } catch (std::bad_weak_ptr& e) {
            LOG_ERROR("[Rpc_Server:start] bad_weak_ptr");
        }
        
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
        
        LOG_DEBUG("RpcServer: shutting down (port {})", port_);
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
    RpcServer() : running_(false), listen_fd_(-1) {}
    // 函数萃取traits（用于lambda推导类型）
    template<typename T>
    struct function_traits : public function_traits<decltype(&T::operator())> {};

    template<typename C, typename Ret, typename... Args>
    struct function_traits<Ret(C::*)(Args...) const> {
        using result_type = Ret;
        using args = std::tuple<Args...>;
    };

    // 内部注册方法（字符串 -> 字符串）
    void registerMethod(const std::string& method, RpcHandler handler) {
        handlers_[method] = std::move(handler);
    }

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
            if (client_fd < 0) {
                continue;
            }
            LOG_INFO("RpcServer: accepted client connection (fd={})", client_fd);
            
            // 为每个客户端启动一个fiber处理连接
            auto conn = std::make_shared<RpcConnection>(client_fd);
            try {
                fiber::Fiber::go([server = shared_from_this(), conn]() {
                    server->handleConnection(conn);
                });
            } catch (std::bad_weak_ptr& e) {
                LOG_ERROR("[Rpc_Server:acceptLoop] bad_weak_ptr");
            }
        }
        LOG_INFO("RpcServer: accept loop exited");
    }
    
    // 处理单个连接
    void handleConnection(RpcConnectionPtr conn) {
        try {
            conn->receiveLoop([server = shared_from_this(), conn](const std::string& payload) {
                server->handleRequest(conn, payload);
            });
        } catch (std::bad_weak_ptr& e) {
            LOG_ERROR("[Rpc_Server:handleConnection] bad_weak_ptr");
        }
    }
    
    // 处理RPC请求
    void handleRequest(RpcConnectionPtr conn, const std::string& payload) {
        RpcRequest request;
        if (!request.deserialize(payload)) {
            LOG_ERROR("RpcServer: failed to decode request");
            return;
        }
        
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
                // 调用处理器（string -> string）
                response.result_data = it->second(request.params_data);
                response.success = true;
            } catch (const std::exception& e) {
                response.success = false;
                response.error = std::string("Exception: ") + e.what();
                LOG_ERROR("RpcServer: handler exception: {}", e.what());
            }
        }
        
        // 发送响应
        conn->send(response.serialize());
    }

private:
    bool running_;
    int listen_fd_;
    uint16_t port_{};
    ServerConfig config_;  // 服务器配置（新增，用于生产模式）
    std::unordered_map<std::string, RpcHandler> handlers_;
};

} // namespace rpc

