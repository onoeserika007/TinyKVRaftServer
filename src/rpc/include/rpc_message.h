#pragma once

#include <json/json.h>
#include <cstdint>
#include <string>
#include <sstream>

// Forward declare Encoder/Decoder
namespace rpc {
    class Encoder;
    class Decoder;
}

namespace rpc {

// RPC请求消息 - 只持有序列化后的字符串
struct RpcRequest {
    uint64_t request_id;      // 请求ID（用于匹配响应）
    std::string method;       // 方法名
    std::string params_data;  // 序列化后的参数数据（字符串形式）
    
    // 序列化为字符串（用于网络传输）
    std::string serialize() const;
    
    // 从字符串反序列化（用于网络接收）
    bool deserialize(const std::string& data);
};

// RPC响应消息 - 只持有序列化后的字符串
struct RpcResponse {
    uint64_t request_id;      // 对应的请求ID
    bool success;             // 是否成功
    std::string result_data;  // 序列化后的返回结果（字符串形式）
    std::string error;        // 错误信息（如果失败）
    
    // 序列化为字符串（用于网络传输）
    std::string serialize() const;
    
    // 从字符串反序列化（用于网络接收）
    bool deserialize(const std::string& data);
};

} // namespace rpc

// Include encoder after struct definitions
#include "encoder.h"

namespace rpc {

// Serializer specializations for RpcRequest and RpcResponse
// 这里的特化是不必要的，因为struct已经定义了偏特化

// Implementation of serialization methods
inline std::string RpcRequest::serialize() const {
    auto encoder = Encoder::New();
    encoder->Encode(*this);
    return encoder->Bytes();
}

inline bool RpcRequest::deserialize(const std::string& data) {
    auto decoder = Decoder::New(data);
    return decoder->Decode(*this);
}

inline std::string RpcResponse::serialize() const {
    auto encoder = Encoder::New();
    encoder->Encode(*this);
    return encoder->Bytes();
}

inline bool RpcResponse::deserialize(const std::string& data) {
    auto decoder = Decoder::New(data);
    return decoder->Decode(*this);
}

} // namespace rpc
