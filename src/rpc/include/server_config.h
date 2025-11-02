#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace rpc {

// 服务注册类型
enum class RegistryType {
    NONE,           // 无服务注册（测试模式）
    STATIC,         // 静态配置文件
    ZOOKEEPER,      // ZooKeeper
    ETCD,           // etcd
    CONSUL,         // Consul
    KUBERNETES      // Kubernetes Service/StatefulSet
};

// RPC服务器配置
struct ServerConfig {
    // 网络配置
    std::string listen_addr = "127.0.0.1";    // 监听地址
    uint16_t port = 0;                         // 端口（0表示自动分配）
    
    // 服务信息
    std::string service_name;                  // 服务名称（如 "raft-node-1"）
    std::map<std::string, std::string> metadata; // 元数据（如 region, zone等）
    
    // 服务注册配置
    RegistryType registry_type = RegistryType::NONE;
    std::vector<std::string> registry_endpoints; // 注册中心地址
    std::string registry_path;                   // 注册路径（如 "/services/raft"）
    int session_timeout_ms = 10000;              // 会话超时
    
    // 健康检查
    int health_check_interval_ms = 5000;         // 健康检查间隔
    
    // 超时配置
    int connect_timeout_ms = 3000;               // 连接超时
    int request_timeout_ms = 5000;               // 请求超时
    
    // 构造函数：简单模式（测试用）
    ServerConfig() = default;
    
    // 构造函数：指定端口（与旧接口兼容）
    explicit ServerConfig(uint16_t p) : port(p) {}
    
    // 从配置文件加载（预留接口）
    static ServerConfig fromFile(const std::string& config_file);
    
    // 从环境变量加载（预留接口）
    static ServerConfig fromEnv();
};

// RPC客户端配置
struct ClientConfig {
    // 目标地址
    std::string target_addr;
    uint16_t target_port = 0;
    
    // 重连策略
    bool auto_reconnect = true;                  // 是否自动重连
    int max_retry_times = 3;                     // 最大重试次数
    int retry_interval_ms = 1000;                // 重试间隔
    
    // 超时配置
    int connect_timeout_ms = 3000;               // 连接超时
    int request_timeout_ms = 5000;               // 请求超时
    
    // 负载均衡策略（预留）
    enum class LoadBalanceMode {
        ROUND_ROBIN,
        RANDOM,
        LEAST_CONN,
        CONSISTENT_HASH
    };
    LoadBalanceMode lb_mode = LoadBalanceMode::ROUND_ROBIN;
    
    // 构造函数
    ClientConfig() = default;
    ClientConfig(const std::string& addr, uint16_t port)
        : target_addr(addr), target_port(port) {}
};

} // namespace rpc

#endif // SERVER_CONFIG_H
