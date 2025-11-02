#include "include/persister.h"
#include "../../third_party/basic_libs/include/logger.h"
#include <fstream>

namespace raft {

// ============================================================================
// DiskPersister 实现（占位，TODO）
// ============================================================================

DiskPersister::DiskPersister(const std::string& data_dir) 
    : data_dir_(data_dir) {
    // TODO: 初始化WAL日志
    // TODO: 创建目录（需要C++17 filesystem或手动mkdir）
    LOG_INFO("DiskPersister initialized at: {}", data_dir_);
}

std::shared_ptr<IPersister> DiskPersister::Copy() {
    std::unique_lock<fiber::FiberMutex> lock(mu_);
    // TODO: 实现真正的复制
    auto np = std::make_shared<DiskPersister>(data_dir_ + "_copy");
    return np;
}

std::vector<uint8_t> DiskPersister::ReadRaftState() {
    std::unique_lock<fiber::FiberMutex> lock(mu_);
    // TODO: 从磁盘读取
    LOG_WARN("DiskPersister::ReadRaftState not implemented yet");
    return std::vector<uint8_t>();
}

int DiskPersister::RaftStateSize() {
    std::unique_lock<fiber::FiberMutex> lock(mu_);
    // TODO: 返回实际大小
    return 0;
}

void DiskPersister::Save(const std::vector<uint8_t>& raftstate, 
                         const std::vector<uint8_t>& snapshot) {
    std::unique_lock<fiber::FiberMutex> lock(mu_);
    // TODO: WAL写入
    // TODO: 快照持久化
    LOG_WARN("DiskPersister::Save not implemented yet (state size: {}, snapshot size: {})", 
             raftstate.size(), snapshot.size());
}

std::vector<uint8_t> DiskPersister::ReadSnapshot() {
    std::unique_lock<fiber::FiberMutex> lock(mu_);
    // TODO: 从磁盘读取快照
    LOG_WARN("DiskPersister::ReadSnapshot not implemented yet");
    return std::vector<uint8_t>();
}

int DiskPersister::SnapshotSize() {
    std::unique_lock<fiber::FiberMutex> lock(mu_);
    // TODO: 返回实际大小
    return 0;
}

} // namespace raft
