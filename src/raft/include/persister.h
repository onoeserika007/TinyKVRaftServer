#ifndef RAFT_PERSISTER_H
#define RAFT_PERSISTER_H

#include "../../fiber/include/sync.h"
#include <vector>
#include <memory>
#include <cstring>
#include <mutex>

namespace raft {

// Persister抽象接口
// 提供Raft状态持久化的统一接口，支持不同实现：
// - MemoryPersister: 内存持久化（测试用，快速，易重置）
// - DiskPersister: 磁盘持久化（生产用，WAL日志）
class IPersister {
public:
    virtual ~IPersister() = default;
    
    // 读取Raft状态
    virtual std::vector<uint8_t> ReadRaftState() = 0;
    
    // 获取Raft状态大小
    virtual int RaftStateSize() = 0;
    
    // 原子保存Raft状态和快照
    virtual void Save(const std::vector<uint8_t>& raftstate, 
                      const std::vector<uint8_t>& snapshot) = 0;
    
    // 读取快照
    virtual std::vector<uint8_t> ReadSnapshot() = 0;
    
    // 获取快照大小
    virtual int SnapshotSize() = 0;
    
    // 复制当前Persister（用于服务器重启时）
    virtual std::shared_ptr<IPersister> Copy() = 0;
};

using PersisterPtr = std::shared_ptr<IPersister>;

// ============================================================================
// MemoryPersister - 内存实现（测试用）
// ============================================================================
class MemoryPersister : public IPersister {
public:
    MemoryPersister() = default;
    
    // 复制当前Persister（用于服务器重启时）
    std::shared_ptr<IPersister> Copy() override {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        auto np = std::make_shared<MemoryPersister>();
        np->raftstate_ = raftstate_;
        np->snapshot_ = snapshot_;
        return np;
    }
    
    // 读取Raft状态
    std::vector<uint8_t> ReadRaftState() override {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        return clone(raftstate_);
    }
    
    // 获取Raft状态大小
    int RaftStateSize() override {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        return raftstate_.size();
    }
    
    // 原子保存Raft状态和快照
    void Save(const std::vector<uint8_t>& raftstate, 
              const std::vector<uint8_t>& snapshot) override {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        raftstate_ = clone(raftstate);
        snapshot_ = clone(snapshot);
    }
    
    // 读取快照
    std::vector<uint8_t> ReadSnapshot() override {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        return clone(snapshot_);
    }
    
    // 获取快照大小
    int SnapshotSize() override {
        std::unique_lock<fiber::FiberMutex> lock(mu_);
        return snapshot_.size();
    }
    
private:
    static std::vector<uint8_t> clone(const std::vector<uint8_t>& orig) {
        return std::vector<uint8_t>(orig.begin(), orig.end());
    }
    
    fiber::FiberMutex mu_;
    std::vector<uint8_t> raftstate_;
    std::vector<uint8_t> snapshot_;
};

// ============================================================================
// DiskPersister - 磁盘实现（生产用，TODO）
// ============================================================================
class DiskPersister : public IPersister {
public:
    explicit DiskPersister(const std::string& data_dir);
    
    std::shared_ptr<IPersister> Copy() override;
    std::vector<uint8_t> ReadRaftState() override;
    int RaftStateSize() override;
    void Save(const std::vector<uint8_t>& raftstate, 
              const std::vector<uint8_t>& snapshot) override;
    std::vector<uint8_t> ReadSnapshot() override;
    int SnapshotSize() override;
    
private:
    std::string data_dir_;
    fiber::FiberMutex mu_;
    // TODO: WAL日志实现
    // TODO: 快照管理
};

// ============================================================================
// 工厂函数
// ============================================================================

// 创建内存Persister（测试用）
inline PersisterPtr MakeMemoryPersister() {
    return std::make_shared<MemoryPersister>();
}

// 创建磁盘Persister（生产用）
inline PersisterPtr MakeDiskPersister(const std::string& data_dir) {
    return std::make_shared<DiskPersister>(data_dir);
}

} // namespace raft

#endif // RAFT_PERSISTER_H
