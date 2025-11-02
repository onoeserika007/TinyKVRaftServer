#include "include/config.h"
#include "include/group.h"
#include "scheduler.h"
#include "fiber.h"
#include "logger.h"
#include "rpc_client_typed_pfr.h"
#include <cassert>

using namespace raft_test;

// 简单的测试RPC请求/响应
struct PingRequest {
    int sender_id;
    std::string message;
};

struct PingResponse {
    int responder_id;
    std::string reply;
    bool success;
};

// 测试服务 - 实现一个简单的Ping RPC
class TestRaftService : public IService {
public:
    TestRaftService(int server_id) : server_id_(server_id), killed_(false) {
        LOG_INFO("TestRaftService {} created", server_id_);
    }
    
    void Kill() override {
        killed_ = true;
        LOG_INFO("TestRaftService {} killed", server_id_);
    }
    
    // 注册RPC方法
    void RegisterRPC(std::shared_ptr<rpc::TypedRpcServer> rpc_server) override {
        // 使用lambda捕获this指针来调用成员函数
        rpc_server->registerHandler(
            "Ping",
            [this](const PingRequest& req, PingResponse& resp) {
                return this->Ping(req, resp);
            }
        );
        LOG_INFO("Server {} registered Ping RPC method", server_id_);
    }
    
    // Ping RPC handler
    std::optional<std::string> Ping(const PingRequest& req, PingResponse& resp) {
        if (killed_) {
            return "service killed";
        }
        
        LOG_INFO("Server {} received Ping from {}: {}", server_id_, req.sender_id, req.message);
        
        resp.responder_id = server_id_;
        resp.reply = "Pong from server " + std::to_string(server_id_);
        resp.success = true;
        
        return std::nullopt;  // no error
    }
    
    int GetServerId() const { return server_id_; }
    bool IsKilled() const { return killed_; }
    
private:
    int server_id_;
    bool killed_;
};

// 启动测试服务器
// 注意：这个函数在ServerGroup::StartServer中被调用
// ServerGroup会负责创建RPC服务器并分配端口
std::vector<ServicePtr> startTestRaftServer(
    const std::vector<ClientEndPtr>& ends,
    int gid,
    int server_id,
    PersisterPtr persister)
{
    LOG_INFO("Starting test Raft server group={} id={} with {} client_ends", 
             gid, server_id, ends.size());
    
    auto svc = std::make_shared<TestRaftService>(server_id);
    
    // 注意：RPC服务器的创建和注册需要在ServerGroup中完成
    // 因为需要知道端口号等网络信息
    
    return {svc};
}

void testPeerCommunication() {
    LOG_INFO("=== Test Peer Communication ===");
    
    // 创建3个peer
    int n = 3;
    auto cfg = std::make_shared<Config>(n, true, startTestRaftServer);
    auto group = cfg->GetGroup();
    
    // 验证所有服务器都已启动
    assert(group->N() == 3);
    for (int i = 0; i < n; i++) {
        assert(group->IsConnected(i));
    }
    
    LOG_INFO("All {} servers started and connected", n);
    
    // 给服务器一点时间启动
    fiber::Fiber::sleep(100);
    
    // 测试peer 0 向 peer 1 发送Ping
    // 注意：我们需要通过ServerGroup获取client_ends
    // 但ServerGroup没有暴露servers_，我们需要添加一个方法
    // 暂时先简单测试：创建一个新的client连接到peer 1
    LOG_INFO("Testing RPC call from peer 0 to peer 1...");
    
    rpc::TypedRpcClient client;
    if (!client.connect("127.0.0.1", 10001)) {
        LOG_ERROR("Failed to connect to peer 1");
        assert(false);
    }
    
    PingRequest req;
    req.sender_id = 0;
    req.message = "Hello from peer 0";
    
    PingResponse resp;
    auto error = client.call("Ping", req, resp);
    
    if (error.has_value()) {
        LOG_ERROR("RPC call failed: {}", error.value());
        assert(false);
    }
    
    LOG_INFO("Received response from server {}: {}", resp.responder_id, resp.reply);
    assert(resp.responder_id == 1);
    assert(resp.success);
    
    client.disconnect();
    
    // 清理
    cfg->Cleanup();
    
    LOG_INFO("✓ Peer communication test passed");
}

FIBER_MAIN() {
    LOG_INFO("================= Peer Communication Test =====================");
    
    testPeerCommunication();
    
    LOG_INFO("\n=== Test PASSED ===");
    
    return 0;
}
