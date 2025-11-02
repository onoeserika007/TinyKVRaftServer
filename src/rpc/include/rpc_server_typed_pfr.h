#ifndef RPC_SERVER_TYPED_PFR_H
#define RPC_SERVER_TYPED_PFR_H

#include "rpc_server.h"
#include "rpc_serializer_pfr.h"
#include "rpc_message.h"
#include <optional>

namespace rpc {

class TypedRpcServer : public RpcServer {
public:
    TypedRpcServer() = default;
    
    // 注册处理器：std::optional<std::string> func(const Input&, Output&)
    // 返回值：std::nullopt = 成功，有值 = 错误消息
    template<typename InputArgs, typename OutputArgs>
    void registerHandler(const std::string& method, 
                        std::optional<std::string> (*func)(const InputArgs&, OutputArgs&)) {
        RpcServer::registerMethod(method, [func](const Json::Value& params) -> Json::Value {
            InputArgs input = Serializer<InputArgs>::deserialize(params[0]);
            OutputArgs output;
            
            auto error = func(input, output);
            if (error.has_value()) {
                throw std::runtime_error(error.value());
            }
            return Serializer<OutputArgs>::serialize(output);
        });
    }
    
    // 注册lambda：std::optional<std::string> func(const Input&, Output&)
    template<typename Func>
    void registerHandler(const std::string& method, Func func) {
        RpcServer::registerMethod(method, [func](const Json::Value& params) -> Json::Value {
            using InputArgs = std::decay_t<std::remove_reference_t<
                std::tuple_element_t<0, typename function_traits<Func>::args>>>;
            using OutputArgs = std::decay_t<std::remove_reference_t<
                std::tuple_element_t<1, typename function_traits<Func>::args>>>;
            
            InputArgs input = Serializer<InputArgs>::deserialize(params[0]);
            OutputArgs output;
            
            auto error = func(input, output);
            if (error.has_value()) {
                throw std::runtime_error(error.value());
            }
            return Serializer<OutputArgs>::serialize(output);
        });
    }
    
private:
    // 函数萃取traits（用于lambda）
    template<typename T>
    struct function_traits : public function_traits<decltype(&T::operator())> {};
    
    template<typename C, typename Ret, typename... Args>
    struct function_traits<Ret(C::*)(Args...) const> {
        using result_type = Ret;
        using args = std::tuple<Args...>;
    };
};

} // namespace rpc

#endif // RPC_SERVER_TYPED_PFR_H
