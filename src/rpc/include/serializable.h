#pragma once

#include <json/json.h>
#include <string>

namespace rpc {

// 序列化接口，用户需要实现此接口
class Serializable {
public:
    virtual ~Serializable() = default;
    
    // 序列化为JSON
    virtual Json::Value toJson() const = 0;
    
    // 从JSON反序列化（子类需要实现静态方法或构造函数）
    // 注意：这里不能定义纯虚静态方法，用户需要自己实现
    // static T fromJson(const Json::Value& json);
};

// 序列化工具函数
class JsonCodec {
public:
    // JSON对象转字符串
    static std::string encode(const Json::Value& json) {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";  // 紧凑格式
        return Json::writeString(writer, json);
    }
    
    // 字符串转JSON对象
    static bool decode(const std::string& str, Json::Value& json) {
        Json::CharReaderBuilder reader;
        std::string errors;
        std::istringstream iss(str);
        return Json::parseFromStream(reader, iss, &json, &errors);
    }
};

} // namespace rpc
