# 生产环境架构设计

## 当前架构 vs 生产环境需求

### 当前测试框架架构
```
测试框架 (Config/ServerGroup)
    ↓
启动函数 (startRaftServer)
    ↓
创建 Raft 实例
    ↓
注册 RPC 方法 (RegisterRPC)
    ↓
TypedRpcServer (固定端口 10000+)
```

~~---~~

### 生产环境架构需求

```
配置中心 (ZooKeeper/etcd/Consul)
    ↓
服务发现 & 配置管理
    ↓
Raft 节点启动
    ↓
注册到配置中心
    ↓
动态发现 Peers
    ↓
建立 RPC 连接
```

**需求**：
- 分布式部署（跨机器）
- 动态服务发现
- 配置热更新
- 健康检查
- 故障转移
- 灰度发布

---

## 关键接口需要修改的地方

### 1. RpcServer - 需要服务注册能力

**当前问题**：
```cpp
class TypedRpcServer {
    void start(uint16_t port);  // 硬编码端口
};
```

**改进方案**：
```cpp
struct ServerConfig {
    std::string service_name;        // 服务名称
    std::string listen_addr;         // 监听地址 (0.0.0.0 或特定网卡)
    uint16_t port;                   // 端口（0表示自动分配）
    
    // 服务发现相关
    std::string registry_type;       // "zookeeper" | "etcd" | "consul" | "none"
    std::vector<std::string> registry_endpoints;  // ["127.0.0.1:2181"]
    std::string registry_path;       // "/services/raft/node-{id}"
    
    // 元数据
    std::map<std::string, std::string> metadata;  // {{"region": "us-west"}}
    int health_check_interval_ms;    // 健康检查间隔
};

class TypedRpcServer {
public:
    // 新的启动接口
    bool start(const ServerConfig& config);
    
    // 获取实际监听的端口（如果配置为0）
    uint16_t getActualPort() const;
    
    // 获取本机IP（用于注册）
    std::string getLocalIP() const;
    
    // 服务注册接口
    bool registerToRegistry();
    bool unregisterFromRegistry();
    
    // 健康检查回调
    using HealthCheckFunc = std::function<bool()>;
    void setHealthCheck(HealthCheckFunc func);
};
```

---

### 2. Raft - 需要动态 Peer 发现

**当前问题**：
```cpp
class Raft {
    // 启动时传入固定的 peers
    Raft(const std::vector<ClientEndPtr>& peers, int me, PersisterPtr persister);
};
```

**改进方案**：
```cpp
struct RaftConfig {
    int node_id;                     // 节点ID
    std::string cluster_name;        // 集群名称
    
    // Peer发现
    enum class PeerDiscoveryMode {
        STATIC,      // 静态配置（测试用）
        REGISTRY,    // 通过服务注册中心
        DNS,         // 通过DNS SRV记录
        KUBERNETES   // 通过K8s StatefulSet
    };
    PeerDiscoveryMode discovery_mode;
    
    // 静态模式：直接配置peer地址
    std::vector<std::string> static_peers;  // ["192.168.1.1:10000", ...]
    
    // 动态模式：注册中心配置
    std::string registry_path;       // "/raft/clusters/mycluster"
    
    // 持久化
    std::string data_dir;            // "/var/lib/raft/node-1"
    
    // Raft参数
    int election_timeout_min_ms;
    int election_timeout_max_ms;
    int heartbeat_interval_ms;
};

class Raft {
public:
    // 新的构造函数 - 延迟Peer发现
    Raft(const RaftConfig& config, PersisterPtr persister);
    
    // 启动时发现并连接Peers
    bool start();
    
    // 动态添加/移除Peer（成员变更）
    void addPeer(const std::string& peer_addr);
    void removePeer(const std::string& peer_addr);
    
    // 获取当前集群成员
    std::vector<std::string> getClusterMembers();
    
private:
    // Peer发现器（抽象接口）
    std::unique_ptr<IPeerDiscovery> peer_discovery_;
    
    // 动态维护的Peer连接
    std::map<std::string, ClientEndPtr> peers_;
};
```

---

### 3. 配置中心集成接口

**新增组件**：
```cpp
// 服务注册接口（抽象）
class IServiceRegistry {
public:
    virtual ~IServiceRegistry() = default;
    
    // 注册服务
    virtual bool registerService(
        const std::string& service_name,
        const std::string& addr,
        uint16_t port,
        const std::map<std::string, std::string>& metadata
    ) = 0;
    
    // 注销服务
    virtual bool unregisterService(const std::string& service_name) = 0;
    
    // 发现服务
    virtual std::vector<ServiceInstance> discoverServices(
        const std::string& service_name
    ) = 0;
    
    // 监听服务变化
    using ServiceChangeCallback = std::function<void(
        const std::string& service_name,
        const std::vector<ServiceInstance>& instances
    )>;
    virtual void watchServices(
        const std::string& service_name,
        ServiceChangeCallback callback
    ) = 0;
    
    // 保持心跳
    virtual bool keepAlive() = 0;
};

// ZooKeeper实现
class ZooKeeperRegistry : public IServiceRegistry {
    // 实现基于ZooKeeper的服务注册
};

// Etcd实现
class EtcdRegistry : public IServiceRegistry {
    // 实现基于etcd的服务注册
};

// Consul实现
class ConsulRegistry : public IServiceRegistry {
    // 实现基于Consul的服务注册
};

// 静态配置（测试用）
class StaticRegistry : public IServiceRegistry {
    // 读取配置文件
};
```

---

### 4. ClientEnd - 需要重连和故障转移

**当前问题**：
```cpp
class ClientEnd {
    // 连接一次，固定地址
    bool Connect();
};
```

**改进方案**：
```cpp
struct ClientConfig {
    std::string target_addr;         // 目标地址
    uint16_t target_port;            // 目标端口
    
    // 重连策略
    bool auto_reconnect;             // 是否自动重连
    int max_retry_times;             // 最大重试次数
    int retry_interval_ms;           // 重试间隔
    int connect_timeout_ms;          // 连接超时
    int request_timeout_ms;          // 请求超时
    
    // 负载均衡（如果目标是多个节点）
    enum class LoadBalanceMode {
        ROUND_ROBIN,
        RANDOM,
        LEAST_CONN,
        CONSISTENT_HASH
    };
    LoadBalanceMode lb_mode;
};

class ClientEnd {
public:
    ClientEnd(const ClientConfig& config);
    
    // 连接管理
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    
    // 自动重连的RPC调用
    template<typename InputArgs, typename OutputArgs>
    bool CallWithRetry(
        const std::string& method,
        const InputArgs& input,
        OutputArgs& output
    );
    
    // 获取连接统计
    struct Stats {
        int total_requests;
        int failed_requests;
        int64_t avg_latency_ms;
        std::chrono::steady_clock::time_point last_success;
    };
    Stats getStats() const;
    
private:
    // 重连逻辑
    bool reconnect();
    
    // 健康检查
    bool healthCheck();
};
```

---

## 生产环境启动流程

### 测试环境 vs 生产环境对比

#### 测试环境（当前）
```cpp
FIBER_MAIN() {
    // 1. 直接创建Config，硬编码3个节点
    auto cfg = std::make_shared<Config>(3, true, startRaftServer);
    
    // 2. 自动分配端口 10000, 10001, 10002
    auto group = cfg->GetGroup();
    
    // 3. 全部在本地启动
    // 4. 使用内存Persister
    // 5. 手动控制网络（ConnectAll/DisconnectAll）
    
    return 0;
}
```

#### 生产环境（建议）
```cpp
int main(int argc, char* argv[]) {
    // 1. 解析命令行参数和配置文件
    Config config = parseConfig(argc, argv);
    // 从: raft.yaml, 环境变量, 命令行参数
    
    // 2. 初始化日志系统
    Logger::init(config.log_level, config.log_file);
    
    // 3. 创建服务注册器
    std::unique_ptr<IServiceRegistry> registry;
    if (config.registry_type == "zookeeper") {
        registry = std::make_unique<ZooKeeperRegistry>(
            config.zk_endpoints
        );
    } else if (config.registry_type == "etcd") {
        registry = std::make_unique<EtcdRegistry>(
            config.etcd_endpoints
        );
    }
    
    // 4. 创建持久化存储
    auto persister = std::make_shared<DiskPersister>(config.data_dir);
    
    // 5. 创建RPC服务器
    ServerConfig server_config;
    server_config.service_name = "raft-node-" + std::to_string(config.node_id);
    server_config.listen_addr = config.listen_addr;
    server_config.port = config.port;
    server_config.registry_type = config.registry_type;
    server_config.registry_endpoints = config.registry_endpoints;
    
    auto rpc_server = std::make_shared<rpc::TypedRpcServer>();
    if (!rpc_server->start(server_config)) {
        LOG_ERROR("Failed to start RPC server");
        return 1;
    }
    
    // 6. 创建Raft节点
    RaftConfig raft_config;
    raft_config.node_id = config.node_id;
    raft_config.cluster_name = config.cluster_name;
    raft_config.discovery_mode = RaftConfig::PeerDiscoveryMode::REGISTRY;
    raft_config.registry_path = "/raft/" + config.cluster_name;
    
    auto raft = std::make_shared<Raft>(raft_config, persister);
    
    // 7. 注册RPC方法
    raft->RegisterRPC(rpc_server);
    
    // 8. 启动Raft（发现Peers并连接）
    if (!raft->start()) {
        LOG_ERROR("Failed to start Raft");
        return 1;
    }
    
    // 9. 注册到服务中心
    if (!rpc_server->registerToRegistry()) {
        LOG_ERROR("Failed to register to registry");
        return 1;
    }
    
    // 10. 启动健康检查
    rpc_server->setHealthCheck([&raft]() {
        return !raft->isKilled();
    });
    
    // 11. 等待信号退出
    waitForShutdown();
    
    // 12. 优雅关闭
    registry->unregisterService(server_config.service_name);
    raft->shutdown();
    rpc_server->shutdown();
    
    return 0;
}
```

---

## 配置文件示例

### raft.yaml
```yaml
# 节点配置
node:
  id: 1
  cluster_name: "my-raft-cluster"
  data_dir: "/var/lib/raft/node-1"

# 网络配置
network:
  listen_addr: "0.0.0.0"
  port: 10000  # 0表示自动分配

# RPC配置
rpc:
  request_timeout_ms: 5000
  connect_timeout_ms: 3000
  max_retry_times: 3
  retry_interval_ms: 1000

# 服务发现配置
discovery:
  mode: "zookeeper"  # zookeeper | etcd | consul | static | dns
  
  # ZooKeeper配置
  zookeeper:
    endpoints:
      - "192.168.1.10:2181"
      - "192.168.1.11:2181"
      - "192.168.1.12:2181"
    session_timeout_ms: 10000
    base_path: "/raft/my-raft-cluster"
  
  # 静态配置（测试用）
  static:
    peers:
      - "192.168.1.1:10000"
      - "192.168.1.2:10000"
      - "192.168.1.3:10000"

# Raft参数
raft:
  election_timeout_min_ms: 150
  election_timeout_max_ms: 300
  heartbeat_interval_ms: 50
  snapshot_interval: 1000
  max_log_size_mb: 100

# 日志配置
logging:
  level: "info"  # debug | info | warn | error
  file: "/var/log/raft/node-1.log"
  max_size_mb: 100
  max_backups: 10
```

---

## 迁移路径

### 阶段1：保持测试框架，添加生产接口（当前阶段）

**目标**：不影响现有测试，逐步添加生产所需接口

1. 保持 `ServerGroup/Config` 用于测试
2. 为 `TypedRpcServer` 添加 `ServerConfig` 构造函数（重载）
3. 为 `Raft` 添加 `RaftConfig` 构造函数（重载）
4. 创建 `IServiceRegistry` 抽象接口

**示例**：
```cpp
class TypedRpcServer {
public:
    // 保留旧接口（测试用）
    void start(uint16_t port);
    
    // 新增生产接口
    bool start(const ServerConfig& config);  // NEW
};
```

---

### 阶段2：实现静态配置模式

**目标**：支持通过配置文件启动，但不依赖ZooKeeper

1. 实现 `StaticRegistry`（读取配置文件）
2. 实现 `DiskPersister`（真正的磁盘持久化）
3. 创建 `raft_server` 可执行文件（生产启动器）
4. 支持命令行参数和配置文件

**启动命令**：
```bash
./raft_server \
  --node-id=1 \
  --config=raft.yaml \
  --data-dir=/var/lib/raft/node-1
```

---

### 阶段3：集成ZooKeeper/etcd

**目标**：真正的动态服务发现

1. 实现 `ZooKeeperRegistry`
2. 实现 `EtcdRegistry`
3. Raft 支持动态成员变更
4. 实现健康检查和自动重连

---

### 阶段4：Kubernetes 原生支持

**目标**：云原生部署

1. ⏳ 创建 Dockerfile
2. ⏳ 创建 Helm Chart
3. ⏳ StatefulSet 部署
4. ⏳ 使用 Headless Service 做服务发现

---

## 立即需要修改的接口（优先级）

### 高优先级（影响架构）

1. **RpcServer 添加配置结构体**
   - 当前：`start(uint16_t port)`
   - 改进：`start(const ServerConfig& config)`
   - 影响：允许灵活配置，不影响现有代码

2. **Raft 构造函数添加配置参数**
   - 当前：传入固定的 `ClientEndPtr` 数组
   - 改进：添加 `RaftConfig`，延迟Peer发现
   - 影响：支持动态Peer发现

3. **创建 IServiceRegistry 接口**
   - 目的：抽象服务注册，便于切换实现
   - 实现：`StaticRegistry`, `ZooKeeperRegistry`, `EtcdRegistry`

### 中优先级（影响可靠性）

4. **ClientEnd 添加重连机制**
   - 当前：连接失败直接返回false
   - 改进：自动重连，退避策略
   - 影响：提高生产环境可靠性

5. **Persister 添加磁盘实现**
   - 当前：内存Persister（测试用）
   - 改进：DiskPersister，WAL日志
   - 影响：真正的持久化

###  低优先级（优化）

6. **添加监控指标**
   - Prometheus metrics
   - 日志结构化
   - Trace支持

7. **配置文件支持**
   - YAML/TOML解析
   - 环境变量覆盖
   - 热更新

---




