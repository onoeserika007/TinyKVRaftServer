#include "config.h"
#include "fiber.h"
#include "logger.h"
#include "scheduler.h"
#include <cassert>

using namespace raft_test;

// 简单的测试服务
class PartitionTestService : public IService {
public:
    PartitionTestService(int id) : id_(id), killed_(false) {
        LOG_INFO("PartitionTestService {} created", id_);
    }
    
    void Kill() override {
        killed_ = true;
        LOG_INFO("PartitionTestService {} killed", id_);
    }
    
    void RegisterRPC(std::shared_ptr<rpc::TypedRpcServer> rpc_server) override {
        // 空实现
    }
    
private:
    int id_;
    bool killed_;
};

std::vector<ServicePtr> startPartitionTestServer(
    const std::vector<ClientEndPtr>& peers,
    int gid,
    int me,
    PersisterPtr persister)
{
    auto service = std::make_shared<PartitionTestService>(me);
    return {service};
}

void testPartialConnection() {
    LOG_INFO("=== Test Partial Connection (Network Partition) ===");
    
    // 创建5个服务器
    int n = 5;
    auto cfg = std::make_shared<Config>(n, true, startPartitionTestServer);
    auto group = cfg->GetGroup();
    
    // 等待服务器启动
    fiber::Fiber::sleep(100);
    
    LOG_INFO("Initial state: all servers connected");
    for (int i = 0; i < n; i++) {
        assert(group->IsConnected(i));
    }
    
    // 模拟网络分区：{0, 1, 2} vs {3, 4}
    // 断开所有连接
    group->DisconnectAll(0);
    group->DisconnectAll(1);
    group->DisconnectAll(2);
    group->DisconnectAll(3);
    group->DisconnectAll(4);
    
    LOG_INFO("All servers disconnected");
    for (int i = 0; i < n; i++) {
        assert(!group->IsConnected(i));
    }
    
    // 重新连接：多数派 {0, 1, 2} 互相连接
    LOG_INFO("Connecting majority partition: {{0, 1, 2}}");
    group->ConnectPeer(0, {1, 2});
    group->ConnectPeer(1, {0, 2});
    group->ConnectPeer(2, {0, 1});
    
    // 验证多数派连接状态
    assert(group->IsConnected(0));
    assert(group->IsConnected(1));
    assert(group->IsConnected(2));
    assert(!group->IsConnected(3));  // 少数派仍然断开
    assert(!group->IsConnected(4));
    
    LOG_INFO("✓ Majority partition {{0, 1, 2}} connected");
    
    // 少数派 {3, 4} 互相连接
    LOG_INFO("Connecting minority partition: {{3, 4}}");
    group->ConnectPeer(3, {4});
    group->ConnectPeer(4, {3});
    
    // 验证少数派连接状态
    assert(group->IsConnected(3));
    assert(group->IsConnected(4));
    
    LOG_INFO("✓ Minority partition {{3, 4}} connected");
    
    // 恢复网络：所有服务器重新连接
    LOG_INFO("Healing partition: connecting all servers");
    group->ConnectAll();
    
    for (int i = 0; i < n; i++) {
        assert(group->IsConnected(i));
    }
    
    LOG_INFO("✓ All servers reconnected");
    
    cfg->Cleanup();
    LOG_INFO("✓ Partial connection test passed");
}

FIBER_MAIN() {
    LOG_INFO("================= Network Partition Test =====================");
    
    testPartialConnection();
    
    LOG_INFO("\n=== Test PASSED ===");
    return 0;
}
