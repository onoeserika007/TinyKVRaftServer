#ifndef RAFT_TEST_NETWORK_H
#define RAFT_TEST_NETWORK_H

#include "sync.h"
#include "channel.h"
#include "fiber.h"
#include "rpc_client_typed_pfr.h"
#include "rpc_server_typed_pfr.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <random>
#include <atomic>
#include <functional>

namespace raft_test {

// 网络延迟常量（毫秒）
const int SHORT_DELAY = 27;
const int LONG_DELAY = 7000;

// 基础端口，从这里开始为每个服务器分配端口
const uint16_t BASE_PORT = 10000;

// RPC请求消息（简化版，用于统计）
struct RpcStats {
    std::atomic<int> count{0};
    std::atomic<int64_t> bytes{0};
};

// 服务器信息
struct ServerInfo {
    std::string servername;
    uint16_t port;
    std::shared_ptr<rpc::TypedRpcServer> rpc_server;
    
    ServerInfo() : port(0) {}
    ServerInfo(const std::string& name, uint16_t p) 
        : servername(name), port(p) {}
};

// ClientEnd - 客户端通信端点
// 对应Go版本的ClientEnd，包装TypedRpcClient
class ClientEnd {
public:
    ClientEnd(const std::string& endname, const std::string& server_addr, uint16_t server_port)
        : endname_(endname)
        , server_addr_(server_addr)
        , server_port_(server_port)
        , enabled_(false)
    {}
    
    // RPC调用 - 对应Go版本的Call()
    template<typename InputArgs, typename OutputArgs>
    bool Call(const std::string& method, const InputArgs& input, OutputArgs& output) {
        if (!enabled_) {
            return false;
        }
        
        // TODO: 在这里添加网络故障模拟
        // 现在先直接调用
        auto error = client_.call(method, input, output);
        return !error.has_value();
    }
    
    void Enable(bool enabled) {
        enabled_ = enabled;
    }
    
    bool IsEnabled() const {
        return enabled_;
    }
    
    const std::string& GetEndname() const {
        return endname_;
    }
    
    bool Connect() {
        if (!client_.connect(server_addr_, server_port_)) {
            return false;
        }
        enabled_ = true;
        return true;
    }
    
    void Disconnect() {
        client_.disconnect();
        enabled_ = false;
    }
    
private:
    std::string endname_;
    std::string server_addr_;
    uint16_t server_port_;
    bool enabled_;
    rpc::TypedRpcClient client_;
};

using ClientEndPtr = std::shared_ptr<ClientEnd>;

// Network - 模拟网络，支持丢包、延迟、分区
// 对应Go版本的Network
class Network {
public:
    Network()
        : reliable_(true)
        , long_delays_(false)
        , long_reordering_(false)
        , next_port_offset_(0)
    {
        // 初始化随机数生成器
        std::random_device rd;
        rng_.seed(rd());
    }
    
    ~Network() {
        Cleanup();
    }
    
    // 分配端口给新服务器
    uint16_t AllocatePort(const std::string& servername) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        uint16_t port = BASE_PORT + next_port_offset_++;
        ServerInfo info(servername, port);
        server_info_[servername] = info;
        return port;
    }
    
    // 创建客户端端点
    ClientEndPtr MakeEnd(const std::string& endname) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        
        // 创建一个占位端点，稍后Connect时会更新地址
        auto end = std::make_shared<ClientEnd>(endname, "127.0.0.1", 0);
        ends_[endname] = end;
        enabled_[endname] = false;
        
        return end;
    }
    
    // 连接端点到服务器
    void Connect(const std::string& endname, const std::string& servername) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        connections_[endname] = servername;
        
        // 更新ClientEnd的目标地址和端口
        auto it = server_info_.find(servername);
        if (it != server_info_.end() && ends_.find(endname) != ends_.end()) {
            // 重新创建ClientEnd指向正确的端口
            auto end = std::make_shared<ClientEnd>(endname, "127.0.0.1", it->second.port);
            ends_[endname] = end;
        }
    }
    
    // 启用/禁用端点
    void SetEnable(const std::string& endname, bool enabled) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        enabled_[endname] = enabled;
        if (auto end = ends_[endname]) {
            end->Enable(enabled);
        }
    }
    
    // 获取服务器端口
    uint16_t GetServerPort(const std::string& servername) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        auto it = server_info_.find(servername);
        if (it != server_info_.end()) {
            return it->second.port;
        }
        return 0;
    }
    
    // 添加服务器
    void AddServer(const std::string& servername, std::shared_ptr<rpc::TypedRpcServer> rpc_server) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        auto it = server_info_.find(servername);
        if (it != server_info_.end()) {
            it->second.rpc_server = rpc_server;
        }
    }
    
    // 删除服务器
    void DeleteServer(const std::string& servername) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        auto it = server_info_.find(servername);
        if (it != server_info_.end()) {
            if (it->second.rpc_server) {
                it->second.rpc_server->shutdown();
                it->second.rpc_server.reset();
            }
        }
        server_info_.erase(servername);
    }
    
    // 设置网络可靠性
    void SetReliable(bool yes) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        reliable_ = yes;
    }
    
    bool IsReliable() {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        return reliable_;
    }
    
    // 设置长延迟
    void LongDelays(bool yes) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        long_delays_ = yes;
    }
    
    // 设置长重排序
    void LongReordering(bool yes) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        long_reordering_ = yes;
    }
    
    // 获取RPC统计
    int GetCount(const std::string& servername) {
        // TODO: 实现per-server统计
        return stats_.count.load();
    }
    
    int GetTotalCount() {
        return stats_.count.load();
    }
    
    int64_t GetTotalBytes() {
        return stats_.bytes.load();
    }
    
    void Cleanup() {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        // 停止所有RPC服务器
        for (auto& pair : server_info_) {
            if (pair.second.rpc_server) {
                pair.second.rpc_server->shutdown();
            }
        }
        ends_.clear();
        server_info_.clear();
        connections_.clear();
        enabled_.clear();
    }
    
private:
    fiber::FiberMutex mu_;
    bool reliable_;
    bool long_delays_;
    bool long_reordering_;
    uint16_t next_port_offset_;
    
    std::unordered_map<std::string, ClientEndPtr> ends_;
    std::unordered_map<std::string, bool> enabled_;
    std::unordered_map<std::string, std::string> connections_;  // endname -> servername
    std::unordered_map<std::string, ServerInfo> server_info_;   // servername -> server info
    
    RpcStats stats_;
    std::mt19937 rng_;
};

using NetworkPtr = std::shared_ptr<Network>;

inline NetworkPtr MakeNetwork() {
    return std::make_shared<Network>();
}

} // namespace raft_test

#endif // RAFT_TEST_NETWORK_H
