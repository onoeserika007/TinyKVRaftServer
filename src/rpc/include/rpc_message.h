#pragma once

#include <json/json.h>
#include <cstdint>
#include <string>
#include <sstream>

namespace rpc {

// RPC请求消息
struct RpcRequest {
    uint64_t request_id;   // 请求ID（用于匹配响应）
    std::string method;    // 方法名
    Json::Value params;    // 参数（JSON格式）
    
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = Json::UInt64(request_id);
        json["method"] = method;
        json["params"] = params;
        return json;
    }
    
    static RpcRequest fromJson(const Json::Value& json) {
        RpcRequest req;
        req.request_id = json["id"].asUInt64();
        req.method = json["method"].asString();
        req.params = json["params"];
        return req;
    }
};

// RPC响应消息
struct RpcResponse {
    uint64_t request_id;   // 对应的请求ID
    bool success;          // 是否成功
    Json::Value result;    // 返回结果
    std::string error;     // 错误信息（如果失败）
    
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = Json::UInt64(request_id);
        json["success"] = success;
        if (success) {
            json["result"] = result;
        } else {
            json["error"] = error;
        }
        return json;
    }
    
    static RpcResponse fromJson(const Json::Value& json) {
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

// JSON编解码工具
class JsonCodec {
public:
    static std::string encode(const Json::Value& json) {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        return Json::writeString(writer, json);
    }
    
    static bool decode(const std::string& str, Json::Value& json) {
        Json::CharReaderBuilder reader;
        std::string errors;
        std::istringstream iss(str);
        return Json::parseFromStream(reader, iss, &json, &errors);
    }
};

} // namespace rpc
