#ifndef RAFT_CONFIG_H
#define RAFT_CONFIG_H

#include <string>
#include <vector>
#include <cstdint>

namespace raft {

// Peer 发现模式
enum class PeerDiscoveryMode {
    STATIC,         // 静态配置（测试模式，直接传入ClientEnd数组）
    CONFIG_FILE,    // 从配置文件读取peer地址
    REGISTRY,       // 通过服务注册中心动态发现
    DNS,            // 通过DNS SRV记录
    KUBERNETES      // 通过Kubernetes StatefulSet/Service
};

// Raft 配置
struct RaftConfig {
    // 节点标识
    int node_id = 0;                            // 节点ID
    std::string cluster_name = "default";       // 集群名称
    
    // Peer 发现配置
    PeerDiscoveryMode discovery_mode = PeerDiscoveryMode::STATIC;
    
    // 静态模式：直接配置peer地址
    std::vector<std::string> static_peers;      // ["192.168.1.1:10000", "192.168.1.2:10000"]
    
    // 动态发现模式：注册中心配置
    std::string registry_path;                  // "/raft/clusters/mycluster"
    std::vector<std::string> registry_endpoints; // ["192.168.1.10:2181"]
    
    // 持久化配置
    std::string data_dir;                       // 数据目录（如 "/var/lib/raft/node-1"）
    bool enable_disk_persist = false;           // 是否启用磁盘持久化（false表示内存模式）
    
    // Raft 算法参数
    int election_timeout_min_ms = 150;          // 选举超时最小值
    int election_timeout_max_ms = 300;          // 选举超时最大值
    int heartbeat_interval_ms = 50;             // 心跳间隔
    
    // 日志管理
    int snapshot_interval = 1000;               // 快照间隔（提交多少条日志后触发）
    int max_log_size_mb = 100;                  // 最大日志大小（MB）
    
    // 性能调优
    int max_append_entries = 100;               // 单次AppendEntries最大条目数
    int apply_batch_size = 100;                 // 应用到状态机的批量大小
    
    // 构造函数：默认配置
    RaftConfig() = default;
    
    // 构造函数：测试模式（node_id）
    explicit RaftConfig(int id) : node_id(id) {}
    
    // 从配置文件加载（预留接口）
    static RaftConfig fromFile(const std::string& config_file);
    
    // 从环境变量加载（预留接口）
    static RaftConfig fromEnv();
    
    // 验证配置有效性
    bool validate() const {
        if (node_id < 0) return false;
        if (election_timeout_min_ms >= election_timeout_max_ms) return false;
        if (heartbeat_interval_ms <= 0) return false;
        if (heartbeat_interval_ms >= election_timeout_min_ms) return false;
        return true;
    }
};

} // namespace raft

#endif // RAFT_CONFIG_H
