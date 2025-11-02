#include "include/config.h"
#include "include/group.h"
#include "scheduler.h"
#include "logger.h"
#include <cassert>

using namespace raft_test;

// 简单的测试服务
class TestService : public IService {
public:
    void Kill() override {
        killed_ = true;
    }
    
    void RegisterRPC(std::shared_ptr<rpc::TypedRpcServer> rpc_server) override {
        // config_test中不需要实际的RPC方法
        (void)rpc_server;
    }
    
    bool IsKilled() const { return killed_; }
    
private:
    bool killed_ = false;
};

// 测试服务启动函数
std::vector<ServicePtr> startTestServer(
    const std::vector<ClientEndPtr>& ends,
    int gid,
    int server_id,
    PersisterPtr persister)
{
    LOG_INFO("Starting test server group={} id={}", gid, server_id);
    auto svc = std::make_shared<TestService>();
    return {svc};
}

void testServerGroup() {
    LOG_INFO("=== Test ServerGroup ===");
    
    auto net = MakeNetwork();
    int n = 3;
    
    // 创建服务器组
    auto group = std::make_shared<ServerGroup>(net, 0, n, startTestServer);
    
    // 验证初始化
    assert(group->N() == 3);
    assert(group->GetServerNames().size() == 3);
    
    // 启动服务器
    group->StartServers();
    
    // 验证连接状态
    for (int i = 0; i < n; i++) {
        assert(group->IsConnected(i));
    }
    
    // 测试断开连接
    group->DisconnectAll(1);
    assert(!group->IsConnected(1));
    
    // 测试重新连接
    group->ConnectOne(1);
    assert(group->IsConnected(1));
    
    // 清理
    group->Cleanup();
    
    LOG_INFO("✓ ServerGroup test passed");
}

void testConfig() {
    LOG_INFO("=== Test Config ===");
    
    // 创建配置
    auto cfg = std::make_shared<Config>(3, true, startTestServer);
    
    // 测试begin/end
    cfg->Begin("Test Config");
    
    // 模拟一些操作
    for (int i = 0; i < 10; i++) {
        cfg->Op();
    }
    
    cfg->End();
    
    // 验证RPC统计
    int total = cfg->RpcTotal();
    LOG_INFO("Total RPCs: {}", total);
    
    // 清理
    cfg->Cleanup();
    
    LOG_INFO("✓ Config test passed");
}

FIBER_MAIN() {
    LOG_INFO("================= Config & ServerGroup Tests =====================");
    
    testServerGroup();
    testConfig();
    
    LOG_INFO("\n=== All Config Tests PASSED ===");
    LOG_INFO("  ✓ ServerGroup: Start/Stop/Connect/Disconnect");
    LOG_INFO("  ✓ Config: Begin/End/Op/RPC stats");
    
    return 0;
}
