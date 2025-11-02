#pragma once

#include "buffer.h"
#include <string>
#include <cstdint>
#include <arpa/inet.h>  // htonl, ntohl

namespace rpc {

// RPC消息格式：
// [4字节长度（网络字节序）][payload]
class Protocol {
public:
    // 编码：添加长度前缀
    static std::string encode(const std::string& payload) {
        uint32_t length = payload.size();
        uint32_t net_length = htonl(length);  // 转换为网络字节序
        
        std::string packet;
        packet.reserve(4 + length);
        packet.append(reinterpret_cast<const char*>(&net_length), 4);
        packet.append(payload);
        
        return packet;
    }
    
    // 解码：从buffer中提取完整消息
    // 返回：true表示成功提取一条消息，false表示数据不完整
    static bool decode(Buffer& buffer, std::string& payload) {
        // 状态1：等待长度字段（4字节）
        if (buffer.readable() < 4) {
            return false;
        }
        
        // 读取长度（不消费）
        uint32_t net_length;
        std::memcpy(&net_length, buffer.peek(), 4);
        uint32_t length = ntohl(net_length);  // 转换为主机字节序
        
        // 状态2：等待完整payload
        if (buffer.readable() < 4 + length) {
            return false;
        }
        
        // 提取完整消息
        buffer.consume(4);  // 消费长度字段
        payload = buffer.retrieve(length);  // 提取payload
        
        return true;
    }
};

} // namespace rpc
