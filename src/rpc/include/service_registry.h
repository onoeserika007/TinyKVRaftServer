#ifndef SERVICE_REGISTRY_H
#define SERVICE_REGISTRY_H

#include "server_config.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace rpc {

// 服务实例信息
struct ServiceInstance {
    std::string service_name;                    // 服务名称
    std::string addr;                            // IP地址
    uint16_t port;                               // 端口
    std::map<std::string, std::string> metadata; // 元数据
    int64_t register_time_ms;                    // 注册时间戳
    
    ServiceInstance() : port(0), register_time_ms(0) {}
    
    ServiceInstance(const std::string& name, const std::string& address, uint16_t p)
        : service_name(name), addr(address), port(p), register_time_ms(0) {}
    
    // 获取完整地址
    std::string getFullAddr() const {
        return addr + ":" + std::to_string(port);
    }
};

// 服务注册接口（抽象基类）
class IServiceRegistry {
public:
    virtual ~IServiceRegistry() = default;
    
    // 注册服务
    // @param service_name 服务名称
    // @param addr 服务地址
    // @param port 服务端口
    // @param metadata 元数据（可选）
    // @return 是否注册成功
    virtual bool registerService(
        const std::string& service_name,
        const std::string& addr,
        uint16_t port,
        const std::map<std::string, std::string>& metadata = {}
    ) = 0;
    
    // 注销服务
    // @param service_name 服务名称
    // @return 是否注销成功
    virtual bool unregisterService(const std::string& service_name) = 0;
    
    // 发现服务（获取所有实例）
    // @param service_name 服务名称
    // @return 服务实例列表
    virtual std::vector<ServiceInstance> discoverServices(
        const std::string& service_name
    ) = 0;
    
    // 服务变化回调函数类型
    using ServiceChangeCallback = std::function<void(
        const std::string& service_name,
        const std::vector<ServiceInstance>& instances
    )>;
    
    // 监听服务变化（Watch机制）
    // @param service_name 服务名称
    // @param callback 变化时的回调函数
    virtual void watchServices(
        const std::string& service_name,
        ServiceChangeCallback callback
    ) = 0;
    
    // 保持心跳（用于临时节点）
    // @return 是否心跳成功
    virtual bool keepAlive() = 0;
    
    // 检查连接状态
    virtual bool isConnected() const = 0;
    
    // 关闭连接
    virtual void close() = 0;
};

// 静态配置注册器（测试用）
// 从配置文件或内存读取，不依赖外部服务
class StaticRegistry : public IServiceRegistry {
public:
    StaticRegistry() = default;
    
    // 设置静态服务列表
    void setServices(const std::string& service_name, 
                    const std::vector<ServiceInstance>& instances) {
        services_[service_name] = instances;
    }
    
    bool registerService(
        const std::string& service_name,
        const std::string& addr,
        uint16_t port,
        const std::map<std::string, std::string>& metadata = {}
    ) override {
        // 静态注册器不需要真正注册
        return true;
    }
    
    bool unregisterService(const std::string& service_name) override {
        // 静态注册器不需要注销
        return true;
    }
    
    std::vector<ServiceInstance> discoverServices(
        const std::string& service_name
    ) override {
        auto it = services_.find(service_name);
        if (it != services_.end()) {
            return it->second;
        }
        return {};
    }
    
    void watchServices(
        const std::string& service_name,
        ServiceChangeCallback callback
    ) override {
        // 静态注册器不支持Watch
    }
    
    bool keepAlive() override {
        return true;
    }
    
    bool isConnected() const override {
        return true;
    }
    
    void close() override {
        // 无需关闭
    }
    
private:
    std::map<std::string, std::vector<ServiceInstance>> services_;
};

// ZooKeeper 注册器（预留接口）
class ZooKeeperRegistry : public IServiceRegistry {
public:
    // TODO: 实现基于ZooKeeper的服务注册
    // 需要引入 ZooKeeper C++ 客户端库
    
    bool registerService(
        const std::string& service_name,
        const std::string& addr,
        uint16_t port,
        const std::map<std::string, std::string>& metadata = {}
    ) override {
        // TODO: 实现
        return false;
    }
    
    bool unregisterService(const std::string& service_name) override {
        // TODO: 实现
        return false;
    }
    
    std::vector<ServiceInstance> discoverServices(
        const std::string& service_name
    ) override {
        // TODO: 实现
        return {};
    }
    
    void watchServices(
        const std::string& service_name,
        ServiceChangeCallback callback
    ) override {
        // TODO: 实现
    }
    
    bool keepAlive() override {
        // TODO: 实现
        return false;
    }
    
    bool isConnected() const override {
        // TODO: 实现
        return false;
    }
    
    void close() override {
        // TODO: 实现
    }
};

// Etcd 注册器（预留接口）
class EtcdRegistry : public IServiceRegistry {
public:
    // TODO: 实现基于etcd的服务注册
    
    bool registerService(
        const std::string& service_name,
        const std::string& addr,
        uint16_t port,
        const std::map<std::string, std::string>& metadata = {}
    ) override {
        return false;
    }
    
    bool unregisterService(const std::string& service_name) override {
        return false;
    }
    
    std::vector<ServiceInstance> discoverServices(
        const std::string& service_name
    ) override {
        return {};
    }
    
    void watchServices(
        const std::string& service_name,
        ServiceChangeCallback callback
    ) override {
    }
    
    bool keepAlive() override {
        return false;
    }
    
    bool isConnected() const override {
        return false;
    }
    
    void close() override {
    }
};

// 创建注册器的工厂方法
inline std::unique_ptr<IServiceRegistry> createRegistry(RegistryType type) {
    switch (type) {
        case RegistryType::STATIC:
            return std::make_unique<StaticRegistry>();
        case RegistryType::ZOOKEEPER:
            return std::make_unique<ZooKeeperRegistry>();
        case RegistryType::ETCD:
            return std::make_unique<EtcdRegistry>();
        default:
            return nullptr;
    }
}

} // namespace rpc

#endif // SERVICE_REGISTRY_H
