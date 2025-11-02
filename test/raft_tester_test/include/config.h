#ifndef RAFT_TEST_CONFIG_H
#define RAFT_TEST_CONFIG_H

#include "group.h"
#include "network.h"
#include <chrono>
#include <string>
#include <atomic>
#include <stdexcept>
#include <iostream>
#include "logger.h"

namespace raft_test {

// Config - 测试配置和统计
class Config {
public:
    Config(int n, bool reliable, StartServerFunc start_func)
        : net_(MakeNetwork())
        , n_(n)
        , start_time_(std::chrono::steady_clock::now())
        , ops_(0)
    {
        net_->SetReliable(reliable);
        
        // 创建服务器组（group id = 0）
        group_ = std::make_shared<ServerGroup>(net_, 0, n, start_func);
        group_->StartServers();
    }
    
    ~Config() {
        Cleanup();
    }
    
    // 设置网络可靠性
    void SetReliable(bool reliable) {
        net_->SetReliable(reliable);
    }
    
    bool IsReliable() {
        return net_->IsReliable();
    }
    
    // 设置长延迟
    void SetLongDelays(bool long_delays) {
        net_->LongDelays(long_delays);
    }
    
    // 设置长重排序
    void SetLongReordering(bool long_reordering) {
        net_->LongReordering(long_reordering);
    }
    
    // 获取服务器组
    ServerGroupPtr GetGroup() {
        return group_;
    }
    
    // 获取RPC统计
    int RpcTotal() {
        return net_->GetTotalCount();
    }
    
    int64_t BytesTotal() {
        return net_->GetTotalBytes();
    }
    
    // 开始测试
    void Begin(const std::string& description) {
        std::string rel = net_->IsReliable() ? "reliable" : "unreliable";
        LOG_INFO("{} ({} network)...", description, rel);
        
        t0_ = std::chrono::steady_clock::now();
        rpcs0_ = RpcTotal();
        ops_.store(0);
    }
    
    // 增加操作计数
    void Op() {
        ops_.fetch_add(1);
    }
    
    // 结束测试
    void End() {
        CheckTimeout();
        
        auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0_).count() / 1000.0;
        int npeers = group_->N();
        int nrpc = RpcTotal() - rpcs0_;
        int ops = ops_.load();
        
        LOG_INFO("  ... Passed -- time {:.1f}s #peers {} #RPCs {} #Ops {}", 
                 t, npeers, nrpc, ops);
    }
    
    // 检查超时（2分钟）
    void CheckTimeout() {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_).count() / 1000.0;
        if (elapsed > 120.0) {
            LOG_ERROR("test took longer than 120 seconds");
            throw std::runtime_error("test timeout");
        }
    }
    
    // 清理资源
    void Cleanup() {
        if (group_) {
            group_->Cleanup();
        }
        if (net_) {
            net_->Cleanup();
        }
        CheckTimeout();
    }
    
private:
    NetworkPtr net_;
    ServerGroupPtr group_;
    int n_;
    
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point t0_;
    int rpcs0_;
    std::atomic<int> ops_;
};

using ConfigPtr = std::shared_ptr<Config>;

} // namespace raft_test

#endif // RAFT_TEST_CONFIG_H
