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

// RPC请求消息
struct RpcRequest {
    uint64_t request_id;   // 请求ID（用于匹配响应）
    std::string method;    // 方法名
    Json::Value params;    // 参数（JSON格式）
    
    // 序列化为字符串
    std::string serialize() const;
    
    // 从字符串反序列化
    bool deserialize(const std::string& data);
};

// RPC响应消息
struct RpcResponse {
    uint64_t request_id;   // 对应的请求ID
    bool success;          // 是否成功
    Json::Value result;    // 返回结果
    std::string error;     // 错误信息（如果失败）
    
    // 序列化为字符串
    std::string serialize() const;
    
    // 从字符串反序列化
    bool deserialize(const std::string& data);
};

} // namespace rpc

// Include encoder after struct definitions
#include "encoder.h"

namespace rpc {

// Serializer specializations for RpcRequest and RpcResponse
template<>
struct Serializer<RpcRequest> {
    static Json::Value serialize(const RpcRequest& req) {
        Json::Value json;
        json["id"] = Json::UInt64(req.request_id);
        json["method"] = req.method;
        json["params"] = req.params;
        return json;
    }
    
    static RpcRequest deserialize(const Json::Value& json) {
        RpcRequest req;
        req.request_id = json["id"].asUInt64();
        req.method = json["method"].asString();
        req.params = json["params"];
        return req;
    }
};

template<>
struct Serializer<RpcResponse> {
    static Json::Value serialize(const RpcResponse& resp) {
        Json::Value json;
        json["id"] = Json::UInt64(resp.request_id);
        json["success"] = resp.success;
        if (resp.success) {
            json["result"] = resp.result;
        } else {
            json["error"] = resp.error;
        }
        return json;
    }
    
    static RpcResponse deserialize(const Json::Value& json) {
        RpcResponse resp;
        resp.request_id = json["id"].asUInt64();
        resp.success = json["success"].asBool();
        if (resp.success) {
            resp.result = json["result"];
        } else {
            resp.error = json["error"].asString();
        }
        return resp;
    }
};

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
