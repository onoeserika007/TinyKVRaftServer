#ifndef RPC_CLIENT_TYPED_PFR_H
#define RPC_CLIENT_TYPED_PFR_H

#include "rpc_client.h"
#include "rpc_serializer_pfr.h"
#include <optional>

namespace rpc {

class TypedRpcClient {
public:
    TypedRpcClient() = default;
    
    bool connect(const std::string& host, uint16_t port) {
        return client_.connect(host, port);
    }
    
    void disconnect() {
        client_.disconnect();
    }
    
    // 统一接口：std::optional<std::string> call(method, input, output)
    // 返回值：std::nullopt=成功，有值=错误消息
    template<typename InputArgs, typename OutputArgs>
    std::optional<std::string> call(const std::string& method, const InputArgs& input, OutputArgs& output) {
        Json::Value params(Json::arrayValue);
        params.append(Serializer<InputArgs>::serialize(input));
        
        RpcResponse response = client_.call(method, params);
        if (!response.success) {
            return response.error;
        }
        
        output = Serializer<OutputArgs>::deserialize(response.result);
        return std::nullopt;
    }
    
private:
    RpcClient client_;
};

} // namespace rpc

#endif // RPC_CLIENT_TYPED_PFR_H
