#ifndef RAFT_TEST_GROUP_H
#define RAFT_TEST_GROUP_H

#include "network.h"
#include "persister.h"  // src/raft/include/persister.h
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include "sync.h"

namespace raft_test {

// 使用 raft 命名空间的 PersisterPtr
using PersisterPtr = raft::PersisterPtr;

// 服务接口 - 必须支持Kill()和RegisterRPC()
class IService {
public:
    virtual ~IService() = default;
    virtual void Kill() = 0;
    
    // 注册RPC方法到服务器
    // 子类实现此方法来注册自己的RPC handlers
    virtual void RegisterRPC(std::shared_ptr<rpc::TypedRpcServer> rpc_server) = 0;
};

using ServicePtr = std::shared_ptr<IService>;

// Server - 单个服务器实例
struct Server {
    PersisterPtr persister;
    std::vector<ClientEndPtr> client_ends;  // 连接到其他服务器的端点
    std::vector<ServicePtr> services;       // 该服务器导出的服务列表
    std::shared_ptr<rpc::TypedRpcServer> rpc_server;  // RPC服务器实例
    uint16_t port;                          // 服务器监听端口
    
    Server() : persister(raft::MakeMemoryPersister()), port(0) {}
};

using ServerPtr = std::shared_ptr<Server>;

// 服务器启动回调函数类型
// 参数: (client_ends, group_id, server_id, persister) -> services
using StartServerFunc = std::function<std::vector<ServicePtr>(
    const std::vector<ClientEndPtr>&, int, int, PersisterPtr)>;

// ServerGroup - 管理一组Raft服务器
class ServerGroup {
public:
    ServerGroup(NetworkPtr net, int gid, int n, StartServerFunc start_func)
        : net_(net)
        , gid_(gid)
        , n_(n)
        , start_func_(start_func)
        , servers_(n)
        , server_names_(n)
        , connected_(n, false)
    {
        // 初始化服务器名称
        for (int i = 0; i < n; i++) {
            server_names_[i] = ServerName(gid, i);
            servers_[i] = std::make_shared<Server>();
        }
    }
    
    ~ServerGroup() {
        Cleanup();
    }
    
    // 生成服务器名称: "server-{gid}-{i}"
    static std::string ServerName(int gid, int i) {
        std::ostringstream oss;
        oss << "server-" << gid << "-" << i;
        return oss.str();
    }
    
    int N() const { return n_; }
    
    const std::vector<std::string>& GetServerNames() const {
        return server_names_;
    }
    
    std::string GetServerName(int i) const {
        return server_names_[i];
    }
    
    bool IsConnected(int i) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        return connected_[i];
    }
    
    // 连接所有服务器
    void ConnectAll() {
        for (int i = 0; i < n_; i++) {
            ConnectOne(i);
        }
    }
    
    // 连接单个服务器
    void ConnectOne(int i) {
        std::vector<int> all(n_);
        for (int j = 0; j < n_; j++) {
            all[j] = j;
        }
        Connect(i, all);
    }
    
    // 断开所有服务器
    void DisconnectAll(int i) {
        std::vector<int> all(n_);
        for (int j = 0; j < n_; j++) {
            all[j] = j;
        }
        Disconnect(i, all);
    }
    
    // 启动服务器
    void StartServer(int i) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        
        auto& srv = servers_[i];
        
        // 1. 分配端口并创建RPC服务器
        srv->port = net_->AllocatePort(server_names_[i]);
        srv->rpc_server = std::make_shared<rpc::TypedRpcServer>();
        
        // 启动RPC服务器（在后台fiber中监听）
        if (!srv->rpc_server->start(srv->port)) {
            LOG_ERROR("Failed to start RPC server for {} on port {}", server_names_[i], srv->port);
            return;
        }
        LOG_INFO("Started RPC server for {} on port {}", server_names_[i], srv->port);
        
        // 2. 创建到其他服务器的客户端端点
        srv->client_ends.clear();
        for (int j = 0; j < n_; j++) {
            std::ostringstream oss;
            oss << "end-" << gid_ << "-" << i << "-" << j;
            std::string endname = oss.str();
            auto end = net_->MakeEnd(endname);
            net_->Connect(endname, server_names_[j]);
            srv->client_ends.push_back(end);
        }
        
        // 3. 调用用户提供的启动函数创建服务
        srv->services = start_func_(srv->client_ends, gid_, i, srv->persister);
        
        // 4. 将RPC服务器注册到network
        net_->AddServer(server_names_[i], srv->rpc_server);
        
        // 5. 让每个服务注册其RPC方法到服务器
        for (auto& svc : srv->services) {
            if (svc) {
                svc->RegisterRPC(srv->rpc_server);
            }
        }
        
        LOG_INFO("Server {} started with {} services", server_names_[i], srv->services.size());
    }
    
    // 启动所有服务器
    void StartServers() {
        for (int i = 0; i < n_; i++) {
            StartServer(i);
        }
        ConnectAll();
    }
    
    // 关闭服务器
    void ShutdownServer(int i) {
        DisconnectAll(i);
        
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        
        net_->DeleteServer(server_names_[i]);
        
        auto& srv = servers_[i];
        if (srv && !srv->services.empty()) {
            for (auto& svc : srv->services) {
                if (svc) {
                    svc->Kill();
                }
            }
            srv->services.clear();
        }
    }
    
    // 关闭所有服务器
    void Shutdown() {
        for (int i = 0; i < n_; i++) {
            ShutdownServer(i);
        }
    }
    
    // 清理资源
    void Cleanup() {
        Shutdown();
    }
    
    // 获取Raft日志大小（跨所有服务器的最大值）
    int LogSize() {
        int max_size = 0;
        for (auto& srv : servers_) {
            if (srv && srv->persister) {
                int size = srv->persister->RaftStateSize();
                if (size > max_size) {
                    max_size = size;
                }
            }
        }
        return max_size;
    }
    
    // 获取快照大小（跨所有服务器的最大值）
    int SnapshotSize() {
        int max_size = 0;
        for (auto& srv : servers_) {
            if (srv && srv->persister) {
                int size = srv->persister->SnapshotSize();
                if (size > max_size) {
                    max_size = size;
                }
            }
        }
        return max_size;
    }
    
    // 连接特定peer到指定的其他peers（用于网络分区测试）
    void ConnectPeer(int i, const std::vector<int>& to) {
        Connect(i, to);
    }

private:
    void Connect(int i, const std::vector<int>& to) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        
        connected_[i] = true;
        
        // 启用outgoing连接: i -> j
        auto& srv = servers_[i];
        if (srv) {
            for (int j : to) {
                if (j < (int)srv->client_ends.size() && srv->client_ends[j]) {
                    srv->client_ends[j]->Enable(true);
                    // 同时通过network启用端点
                    std::ostringstream oss;
                    oss << "end-" << gid_ << "-" << i << "-" << j;
                    net_->SetEnable(oss.str(), true);
                }
            }
        }
        
        // 启用incoming连接: j -> i
        for (int j : to) {
            if (connected_[j] && servers_[j]) {
                if (i < (int)servers_[j]->client_ends.size() && servers_[j]->client_ends[i]) {
                    servers_[j]->client_ends[i]->Enable(true);
                    // 同时通过network启用端点
                    std::ostringstream oss;
                    oss << "end-" << gid_ << "-" << j << "-" << i;
                    net_->SetEnable(oss.str(), true);
                }
            }
        }
    }
    
    void Disconnect(int i, const std::vector<int>& from) {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        
        connected_[i] = false;
        
        // 禁用outgoing连接: i -> j
        auto& srv = servers_[i];
        if (srv) {
            for (int j : from) {
                if (j < (int)srv->client_ends.size() && srv->client_ends[j]) {
                    srv->client_ends[j]->Enable(false);
                    // 同时通过network禁用端点
                    std::ostringstream oss;
                    oss << "end-" << gid_ << "-" << i << "-" << j;
                    net_->SetEnable(oss.str(), false);
                }
            }
        }
        
        // 禁用incoming连接: j -> i
        for (int j : from) {
            if (servers_[j]) {
                if (i < (int)servers_[j]->client_ends.size() && servers_[j]->client_ends[i]) {
                    servers_[j]->client_ends[i]->Enable(false);
                    // 同时通过network禁用端点
                    std::ostringstream oss;
                    oss << "end-" << gid_ << "-" << j << "-" << i;
                    net_->SetEnable(oss.str(), false);
                }
            }
        }
    }
    
    NetworkPtr net_;
    int gid_;
    int n_;
    StartServerFunc start_func_;
    
    std::vector<ServerPtr> servers_;
    std::vector<std::string> server_names_;
    std::vector<bool> connected_;
    
    fiber::FiberMutex mu_;
};

using ServerGroupPtr = std::shared_ptr<ServerGroup>;

} // namespace raft_test

#endif // RAFT_TEST_GROUP_H
