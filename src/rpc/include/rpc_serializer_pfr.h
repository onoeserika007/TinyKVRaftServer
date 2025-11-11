#ifndef RPC_SERIALIZER_PFR_H
#define RPC_SERIALIZER_PFR_H

#include <json/json.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <type_traits>
#include <boost/pfr.hpp>

namespace rpc {

// 第二个参数位置void作为默认值带入偏特化模板求值
// 模板参数可以有冗余，只要第二个模板参数没有被使用，不管它是不是匿名的
template<typename T, typename Partial = void>
struct Serializer;

// int
template<>
struct Serializer<int> {
    static Json::Value serialize(int value) {
        return Json::Value(value);
    }
    static int deserialize(const Json::Value& json) {
        return json.asInt();
    }
};

// uint8_t
template<>
struct Serializer<uint8_t> {
    static Json::Value serialize(uint8_t value) {
        return Json::Value(static_cast<unsigned int>(value));
    }
    static uint8_t deserialize(const Json::Value& json) {
        return static_cast<uint8_t>(json.asUInt());
    }
};

// uint64_t
template<>
struct Serializer<uint64_t> {
    static Json::Value serialize(uint64_t value) {
        return Json::Value(static_cast<Json::UInt64>(value));
    }
    static uint64_t deserialize(const Json::Value& json) {
        return json.asUInt64();
    }
};

// float
template<>
struct Serializer<float> {
    static Json::Value serialize(float value) {
        return Json::Value(value);
    }
    static float deserialize(const Json::Value& json) {
        return json.asFloat();
    }
};

// double
template<>
struct Serializer<double> {
    static Json::Value serialize(double value) {
        return Json::Value(value);
    }
    static double deserialize(const Json::Value& json) {
        return json.asDouble();
    }
};

// bool
template<>
struct Serializer<bool> {
    static Json::Value serialize(bool value) {
        return Json::Value(value);
    }
    static bool deserialize(const Json::Value& json) {
        return json.asBool();
    }
};

// std::string
template<>
struct Serializer<std::string> {
    static Json::Value serialize(const std::string& value) {
        return Json::Value(value);
    }
    static std::string deserialize(const Json::Value& json) {
        return json.asString();
    }
};

// 容器类型
template<typename T>
struct Serializer<std::vector<T>> {
    static Json::Value serialize(const std::vector<T>& vec) {
        Json::Value arr(Json::arrayValue);
        for (const auto& item : vec) {
            arr.append(Serializer<T>::serialize(item));
        }
        return arr;
    }
    
    static std::vector<T> deserialize(const Json::Value& json) {
        std::vector<T> vec;
        for (const auto& item : json) {
            vec.push_back(Serializer<T>::deserialize(item));
        }
        return vec;
    }
};

template<typename K, typename V>
struct Serializer<std::map<K, V>> {
    static Json::Value serialize(const std::map<K, V>& m) {
        Json::Value obj(Json::objectValue);
        for (const auto& [key, value] : m) {
            // 键必须转换为字符串
            std::string key_str;
            if constexpr (std::is_same_v<K, std::string>) {
                key_str = key;
            } else {
                key_str = std::to_string(key);
            }
            obj[key_str] = Serializer<V>::serialize(value);
        }
        return obj;
    }
    
    static std::map<K, V> deserialize(const Json::Value& json) {
        std::map<K, V> m;
        for (const auto& key : json.getMemberNames()) {
            K k;
            if constexpr (std::is_same_v<K, std::string>) {
                k = key;
            } else {
                k = static_cast<K>(std::stoi(key));
            }
            m[k] = Serializer<V>::deserialize(json[key]);
        }
        return m;
    }
};

template<typename K, typename V>
struct Serializer<std::unordered_map<K, V>> {
    static Json::Value serialize(const std::unordered_map<K, V>& m) {
        Json::Value obj(Json::objectValue);
        for (const auto& [key, value] : m) {
            std::string key_str;
            if constexpr (std::is_same_v<K, std::string>) {
                key_str = key;
            } else {
                key_str = std::to_string(key);
            }
            obj[key_str] = Serializer<V>::serialize(value);
        }
        return obj;
    }
    
    static std::unordered_map<K, V> deserialize(const Json::Value& json) {
        std::unordered_map<K, V> m;
        for (const auto& key : json.getMemberNames()) {
            K k;
            if constexpr (std::is_same_v<K, std::string>) {
                k = key;
            } else {
                k = static_cast<K>(std::stoi(key));
            }
            m[k] = Serializer<V>::deserialize(json[key]);
        }
        return m;
    }
};

template<typename T>
struct Serializer<std::optional<T>> {
    static Json::Value serialize(const std::optional<T>& opt) {
        if (opt.has_value()) {
            return Serializer<T>::serialize(*opt);
        }
        return Json::Value(Json::nullValue);
    }
    
    static std::optional<T> deserialize(const Json::Value& json) {
        if (json.isNull()) {
            return std::nullopt;
        }
        return Serializer<T>::deserialize(json);
    }
};

// 聚合类型（struct）- 使用 Boost.PFR 自动反射
template<typename T>
struct Serializer<T, std::enable_if_t<std::is_aggregate_v<T> && !std::is_array_v<T>>> {
    static Json::Value serialize(const T& value) {
        Json::Value arr(Json::arrayValue);
        boost::pfr::for_each_field(value, [&arr](const auto& field) {
            arr.append(Serializer<std::decay_t<decltype(field)>>::serialize(field));
        });
        return arr;
    }
    
    static T deserialize(const Json::Value& json) {
        T result{};
        std::size_t idx = 0;
        boost::pfr::for_each_field(result, [&json, &idx](auto& field) {
            if (idx < json.size()) {
                field = Serializer<std::decay_t<decltype(field)>>::deserialize(
                    json[static_cast<Json::ArrayIndex>(idx)]);
            }
            ++idx;
        });
        return result;
    }
};

} // namespace rpc

#endif // RPC_SERIALIZER_PFR_H
