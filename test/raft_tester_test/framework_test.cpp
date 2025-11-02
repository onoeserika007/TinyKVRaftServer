#include "persister.h"  // src/raft/include/persister.h
#include "include/network.h"
#include "scheduler.h"
#include "fiber.h"
#include "logger.h"
#include <cassert>

using namespace raft_test;

void testPersister() {
    LOG_INFO("=== Test Persister ===");
    
    // 测试基本读写 - 使用 raft::MemoryPersister
    auto ps = raft::MakeMemoryPersister();
    
    std::vector<uint8_t> raftstate = {1, 2, 3, 4, 5};
    std::vector<uint8_t> snapshot = {10, 20, 30};
    
    ps->Save(raftstate, snapshot);
    
    auto read_state = ps->ReadRaftState();
    auto read_snap = ps->ReadSnapshot();
    
    assert(read_state.size() == 5);
    assert(read_snap.size() == 3);
    assert(ps->RaftStateSize() == 5);
    assert(ps->SnapshotSize() == 3);
    
    // 测试Copy
    auto ps2 = ps->Copy();
    auto read_state2 = ps2->ReadRaftState();
    assert(read_state2.size() == 5);
    
    LOG_INFO("✓ Persister test passed");
}

void testNetwork() {
    LOG_INFO("=== Test Network ===");
    
    auto net = MakeNetwork();
    
    // 测试基本功能
    auto end1 = net->MakeEnd("end1");
    assert(end1 != nullptr);
    
    net->Connect("end1", "server1");
    net->SetEnable("end1", true);
    
    assert(net->GetTotalCount() == 0);
    
    net->SetReliable(false);
    assert(!net->IsReliable());
    
    net->Cleanup();
    
    LOG_INFO("✓ Network test passed");
}

FIBER_MAIN() {
    LOG_INFO("================= Raft Test Framework Unit Tests =====================");
    
    testPersister();
    testNetwork();
    
    LOG_INFO("\n=== All Framework Tests PASSED ===");
    LOG_INFO("  ✓ Persister: Read/Write/Copy");
    LOG_INFO("  ✓ Network: Basic setup");
    
    return 0;
}
