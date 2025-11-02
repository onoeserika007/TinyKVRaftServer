# Raft测试框架文档

## 概述

本测试框架用于测试分布式Raft共识算法的实现。它模拟了一个多节点的分布式环境，支持网络分区、消息延迟/丢失等故障注入，使得我们能够在单机环境中测试复杂的分布式场景。

该框架参考MIT 6.5840 (原6.824) Lab2的Go版本实现，但使用C++重写，并适配了我们的Fiber协程框架和TypedRPC系统。

---

## 核心组件

### 1. Persister (持久化层)

**文件**: `include/persister.h`

**作用**: 模拟Raft节点的持久化存储（内存实现）

**关键特性**:
- 保存Raft状态（currentTerm、votedFor、log[]等）
- 保存快照（snapshot）
- 支持Copy操作（用于模拟服务器重启）
- 线程安全（使用FiberMutex）

**主要接口**:
```cpp
class Persister {
    // 读取Raft状态
    std::vector<uint8_t> ReadRaftState();
    
    // 保存Raft状态和快照
    void Save(const std::vector<uint8_t>& raftstate, 
              const std::vector<uint8_t>& snapshot);
    
    // 读取快照
    std::vector<uint8_t> ReadSnapshot();
    
    // 创建副本（用于服务器重启）
    std::shared_ptr<Persister> Copy();
    
    // 获取大小统计
    int RaftStateSize();
    int SnapshotSize();
};
```

**使用示例**:
```cpp
auto ps = MakePersister();
std::vector<uint8_t> state = {1, 2, 3};
ps->Save(state, {});
auto read = ps->ReadRaftState(); // 读回状态
```

---

### 2. Network (网络层)

**文件**: `include/network.h`

**作用**: 模拟分布式网络环境，管理RPC服务器和客户端

**关键特性**:
- 为每个peer分配唯一端口（从10000开始）
- 管理RPC服务器的生命周期
- 支持网络故障注入（可靠/不可靠模式）
- 统计RPC调用次数和字节数

**主要接口**:
```cpp
class Network {
    // 为服务器分配端口
    uint16_t AllocatePort(const std::string& servername);
    
    // 创建客户端端点
    ClientEndPtr MakeEnd(const std::string& endname);
    
    // 连接端点到服务器
    void Connect(const std::string& endname, const std::string& servername);
    
    // 启用/禁用端点
    void Enable(const std::string& endname, bool enabled);
    
    // 添加/删除服务器
    void AddServer(const std::string& servername, 
                   std::shared_ptr<rpc::TypedRpcServer> rpc_server);
    void DeleteServer(const std::string& servername);
    
    // 设置网络特性
    void Reliable(bool yes);        // 是否可靠
    void LongDelays(bool yes);      // 是否有长延迟
    void LongReordering(bool yes);  // 是否有消息重排序
    
    // 获取统计信息
    int GetTotalCount();            // 总RPC次数
    int64_t GetTotalBytes();        // 总字节数
};
```

**网络拓扑管理**:
```cpp
auto net = MakeNetwork();

// 端点名称格式: "end-{groupId}-{fromServer}-{toServer}"
// 例如: "end-0-1-2" 表示从server-0-1到server-0-2的端点
auto end = net->MakeEnd("end-0-1-2");
net->Connect("end-0-1-2", "server-0-2");
net->Enable("end-0-1-2", true);
```

---

### 3. ClientEnd (客户端端点)

**文件**: `include/network.h`

**作用**: RPC客户端，用于向其他peer发送RPC请求

**关键接口**:
```cpp
class ClientEnd {
    // RPC调用
    template<typename InputArgs, typename OutputArgs>
    bool Call(const std::string& method, 
              const InputArgs& input, 
              OutputArgs& output);
    
    // 连接/断开
    bool Connect();
    void Disconnect();
    
    // 启用/禁用
    void Enable(bool enabled);
    bool IsEnabled() const;
};
```

**使用示例**:
```cpp
// 在Raft中发送RequestVote RPC
RequestVoteArgs args;
args.term = currentTerm;
args.candidateId = me;

RequestVoteReply reply;
bool ok = peers[server]->Call("RequestVote", args, reply);
if (ok) {
    // 处理响应
}
```

---

### 4. ServerGroup (服务器组)

**文件**: `include/group.h`

**作用**: 管理一组Raft服务器的生命周期

**关键特性**:
- 自动创建和启动RPC服务器
- 为每个服务器分配端口
- 管理服务器之间的网络连接
- 支持服务器的启动/关闭/重启
- 模拟网络分区（通过断开连接）

**主要接口**:
```cpp
class ServerGroup {
    // 启动/关闭服务器
    void StartServer(int i);
    void ShutdownServer(int i);
    void StartServers();        // 启动所有
    void Shutdown();            // 关闭所有
    
    // 网络拓扑控制
    void ConnectAll();          // 全连接
    void ConnectOne(int i);     // 连接单个服务器到所有其他服务器
    void DisconnectAll(int i);  // 断开单个服务器
    void Connect(int i, std::vector<int> to);      // 连接到指定服务器
    void Disconnect(int i, std::vector<int> from); // 断开指定连接
    
    // 查询状态
    int N() const;                           // 服务器总数
    bool IsConnected(int i);                 // 是否已连接
    const std::vector<std::string>& GetServerNames() const;
    
    // 统计信息
    int LogSize();      // 所有服务器中最大的日志大小
    int SnapshotSize(); // 所有服务器中最大的快照大小
};
```

**服务器生命周期**:
```cpp
// 1. 分配端口
srv->port = net->AllocatePort(servername);

// 2. 创建RPC服务器
srv->rpc_server = std::make_shared<rpc::TypedRpcServer>();
srv->rpc_server->start(srv->port);

// 3. 创建到其他服务器的客户端端点
for (int j = 0; j < n; j++) {
    auto end = net->MakeEnd(endname);
    net->Connect(endname, server_names[j]);
    srv->client_ends.push_back(end);
}

// 4. 调用用户提供的启动函数创建服务
srv->services = start_func(srv->client_ends, gid, i, srv->persister);

// 5. 让服务注册RPC方法
for (auto& svc : srv->services) {
    svc->RegisterRPC(srv->rpc_server);
}
```

---

### 5. IService (服务接口)

**文件**: `include/group.h`

**作用**: 所有Raft服务必须实现的接口

**接口定义**:
```cpp
class IService {
public:
    virtual ~IService() = default;
    
    // 杀死服务（模拟节点崩溃）
    virtual void Kill() = 0;
    
    // 注册RPC方法到服务器
    virtual void RegisterRPC(std::shared_ptr<rpc::TypedRpcServer> rpc_server) = 0;
};
```

**实现示例** (来自peer_communication_test.cpp):
```cpp
class TestRaftService : public IService {
public:
    void Kill() override {
        killed_ = true;
    }
    
    void RegisterRPC(std::shared_ptr<rpc::TypedRpcServer> rpc_server) override {
        // 注册Ping方法
        rpc_server->registerHandler(
            "Ping",
            [this](const PingRequest& req, PingResponse& resp) {
                return this->Ping(req, resp);
            }
        );
    }
    
    // RPC处理函数
    std::optional<std::string> Ping(const PingRequest& req, PingResponse& resp) {
        if (killed_) {
            return "service killed";
        }
        resp.reply = "Pong";
        return std::nullopt;  // 无错误
    }
    
private:
    bool killed_;
};
```

**RPC方法签名规范**:
```cpp
// 所有RPC方法必须遵循此签名：
std::optional<std::string> MethodName(const RequestType& req, ResponseType& resp)

// 返回值：
// - std::nullopt: 成功
// - "error message": 失败，包含错误信息
```

---

### 6. Config (测试配置)

**文件**: `include/config.h`

**作用**: 高层测试配置和统计

**关键特性**:
- 封装ServerGroup和Network
- 提供测试的Begin/End标记
- 统计测试时间、RPC次数等
- 检查测试超时（2分钟）

**主要接口**:
```cpp
class Config {
    // 构造：创建n个服务器，reliable表示是否可靠网络
    Config(int n, bool reliable, StartServerFunc start_func);
    
    // 测试标记
    void Begin(const std::string& description);
    void End();
    
    // 操作计数
    void Op();  // 增加操作计数
    
    // 统计
    int RpcTotal();
    int64_t BytesTotal();
    
    // 网络控制
    void SetReliable(bool reliable);
    bool IsReliable();
    void SetLongDelays(bool long_delays);
    void SetLongReordering(bool long_reordering);
    
    // 获取底层对象
    ServerGroupPtr GetGroup();
    
    // 清理
    void Cleanup();
    void CheckTimeout();
};
```

**使用示例**:
```cpp
// 创建配置：3个服务器，可靠网络
auto cfg = std::make_shared<Config>(3, true, startRaftServer);

cfg->Begin("Test Initial Election");

// ... 测试代码 ...

cfg->End();  // 输出统计信息
cfg->Cleanup();
```

---

## StartServerFunc 回调函数

**类型定义**:
```cpp
using StartServerFunc = std::function<std::vector<ServicePtr>(
    const std::vector<ClientEndPtr>& client_ends,  // 到其他服务器的端点
    int group_id,                                   // 组ID
    int server_id,                                  // 服务器ID
    PersisterPtr persister                          // 持久化存储
)>;
```

**作用**: 用户提供的回调函数，用于创建Raft服务实例

**实现示例**:
```cpp
std::vector<ServicePtr> startRaftServer(
    const std::vector<ClientEndPtr>& peers,
    int gid,
    int me,
    PersisterPtr persister)
{
    // 创建Raft实例
    auto rf = std::make_shared<Raft>(peers, me, persister);
    
    // 返回服务列表（可以有多个服务）
    return {rf};
}
```

---

## 完整测试流程

### 1. 基础测试示例

```cpp
FIBER_MAIN() {
    // 1. 创建配置
    int n = 3;  // 3个服务器
    auto cfg = std::make_shared<Config>(n, true, startRaftServer);
    auto group = cfg->GetGroup();
    
    // 2. 开始测试
    cfg->Begin("Test Initial Election");
    
    // 3. 等待选举
    fiber::Fiber::sleep(1000);  // 等1秒
    
    // 4. 检查是否选出了leader
    int leader1 = checkOneLeader(cfg);
    assert(leader1 >= 0 && leader1 < n);
    
    // 5. 结束测试
    cfg->End();
    
    // 6. 清理
    cfg->Cleanup();
    
    return 0;
}
```

### 2. 网络分区测试示例

```cpp
// 测试网络分区后的行为
void testNetworkPartition() {
    auto cfg = std::make_shared<Config>(5, true, startRaftServer);
    auto group = cfg->GetGroup();
    
    cfg->Begin("Test Network Partition");
    
    // 检查初始leader
    int leader1 = checkOneLeader(cfg);
    
    // 制造分区：将网络分成两部分 {0,1,2} 和 {3,4}
    group->DisconnectAll(3);
    group->DisconnectAll(4);
    
    // 多数派{0,1,2}应该能继续工作
    fiber::Fiber::sleep(2000);
    int leader2 = checkOneLeader(cfg);
    assert(leader2 >= 0 && leader2 <= 2);  // leader应该在多数派
    
    // 恢复网络
    group->ConnectAll();
    
    // 检查系统恢复
    fiber::Fiber::sleep(1000);
    checkOneLeader(cfg);
    
    cfg->End();
    cfg->Cleanup();
}
```

### 3. 服务器崩溃测试示例

```cpp
// 测试leader崩溃后的恢复
void testLeaderCrash() {
    auto cfg = std::make_shared<Config>(3, true, startRaftServer);
    auto group = cfg->GetGroup();
    
    cfg->Begin("Test Leader Crash");
    
    // 找到leader
    int leader1 = checkOneLeader(cfg);
    
    // 杀死leader
    group->ShutdownServer(leader1);
    
    // 等待新leader选举
    fiber::Fiber::sleep(2000);
    
    // 应该选出新leader
    int leader2 = checkOneLeader(cfg);
    assert(leader2 != leader1);  // 新leader不能是旧的
    
    // 重启旧leader
    group->StartServer(leader1);
    group->ConnectOne(leader1);
    
    cfg->End();
    cfg->Cleanup();
}
```

---

## 辅助函数

### checkOneLeader

**作用**: 检查集群中是否有且仅有一个leader

**实现思路**:
```cpp
int checkOneLeader(ConfigPtr cfg) {
    auto group = cfg->GetGroup();
    int n = group->N();
    
    std::map<int, std::vector<int>> leaders;  // term -> [servers]
    
    // 询问每个服务器
    for (int i = 0; i < n; i++) {
        if (!group->IsConnected(i)) continue;
        
        // 调用GetState RPC获取(term, isLeader)
        GetStateReply reply;
        auto end = /* 获取到server i的端点 */;
        bool ok = end->Call("GetState", GetStateArgs{}, reply);
        
        if (ok && reply.isLeader) {
            leaders[reply.term].push_back(i);
        }
    }
    
    int lastTermWithLeader = -1;
    for (auto& [term, servers] : leaders) {
        if (servers.size() > 1) {
            LOG_ERROR("term {} has {} leaders", term, servers.size());
            throw std::runtime_error("multiple leaders in same term");
        }
        lastTermWithLeader = term;
    }
    
    if (lastTermWithLeader < 0) {
        return -1;  // 没有leader
    }
    
    return leaders[lastTermWithLeader][0];
}
```

### checkNoLeader

**作用**: 检查集群中没有leader（用于少数派测试）

---

## 类型系统对比：Go vs C++

### Go版本的特点

1. **接口鸭子类型**:
```go
// Go: 任何有Start()方法的类型都实现了这个接口
type Raft interface {
    Start(command interface{}) (int, int, bool)
}
```

2. **interface{}万能类型**:
```go
// Go: 可以传递任意类型
func Call(method string, args interface{}, reply interface{}) bool
```

3. **反射**:
```go
// Go: 运行时通过反射调用方法
method := reflect.ValueOf(service).MethodByName(methodName)
method.Call(args)
```

### C++版本的特点

1. **显式接口继承**:
```cpp
// C++: 必须显式继承并实现接口
class Raft : public IService {
public:
    void Kill() override;
    void RegisterRPC(...) override;
};
```

2. **模板和类型安全**:
```cpp
// C++: 编译期类型检查
template<typename InputArgs, typename OutputArgs>
bool Call(const std::string& method, const InputArgs& input, OutputArgs& output);
```

3. **lambda + function traits**:
```cpp
// C++: 通过lambda包装成员函数
rpc_server->registerHandler(
    "RequestVote",
    [this](const RequestVoteArgs& args, RequestVoteReply& reply) {
        return this->RequestVote(args, reply);
    }
);
```


- Go更灵活，代码更简洁，但类型安全较弱
- C++类型安全更强，编译期发现错误，但需要更多模板代码

---

## Questions

### Q1: 为什么要使用FIBER_MAIN()？

**A**: 因为我们的RPC系统基于fiber协程实现。FIBER_MAIN()会启动scheduler，让所有IO操作在协程中执行，避免阻塞。

```cpp
// 错误：不使用FIBER_MAIN
int main() {
    auto cfg = Config(3, true, startRaftServer);  // 崩溃！
}

// 正确：使用FIBER_MAIN
FIBER_MAIN() {
    auto cfg = Config(3, true, startRaftServer);  // OK
    return 0;
}
```

### Q2: 为什么RPC方法要返回std::optional<std::string>？

**A**: 这是错误处理机制：
- `std::nullopt`: RPC成功
- `"error message"`: RPC失败，包含错误原因

```cpp
std::optional<std::string> RequestVote(const RequestVoteArgs& args, 
                                       RequestVoteReply& reply) {
    if (killed_) {
        return "server is dead";  // 返回错误
    }
    
    // 正常处理
    reply.voteGranted = true;
    return std::nullopt;  // 成功
}
```

### Q3: 如何模拟网络分区？

**A**: 通过断开服务器连接：

```cpp
// 分区1: {0, 1}，分区2: {2, 3, 4}
group->DisconnectAll(0);
group->DisconnectAll(1);
group->Connect(0, {1});  // 0只能连接到1
group->Connect(1, {0});  // 1只能连接到0
```

### Q4: 为什么需要Persister？

**A**: Raft需要持久化以下数据来保证正确性：
- `currentTerm`: 当前任期
- `votedFor`: 投票给谁
- `log[]`: 日志条目

模拟服务器重启时：
```cpp
// 保存状态
persister->Save(raftstate, snapshot);

// 模拟重启：关闭旧服务器，创建新服务器
group->ShutdownServer(i);
group->StartServer(i);  // 使用同一个persister，状态得以恢复
```

### Q5: 端口是如何分配的？

**A**: Network从BASE_PORT(10000)开始顺序分配：
- server-0-0: 10000
- server-0-1: 10001
- server-0-2: 10002
- ...

所有服务器都在localhost(127.0.0.1)上。

