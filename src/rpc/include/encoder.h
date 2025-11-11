#ifndef RPC_ENCODER_H
#define RPC_ENCODER_H

#include "rpc_serializer_pfr.h"
#include <json/json.h>
#include <string>
#include <sstream>
#include <vector>
#include <memory>

namespace rpc {

/**
 * @brief Go-style Encoder for sequential encoding of multiple values
 * 
 * Usage:
 *   auto encoder = Encoder::New();
 *   encoder->Encode(term);
 *   encoder->Encode(votedFor);
 *   encoder->Encode(logs);
 *   std::string data = encoder->Bytes();
 */
class Encoder {
public:
    using ptr = std::shared_ptr<Encoder>;
    
    /**
     * @brief Create a new encoder instance
     */
    static ptr New() {
        return std::shared_ptr<Encoder>(new Encoder());
    }
    
    /**
     * @brief Encode a value and append to internal buffer
     * @tparam T Type of value to encode (must be supported by Serializer)
     * @param value The value to encode
     */
    template<typename T>
    void Encode(const T& value) {
        Json::Value json_value = Serializer<T>::serialize(value);
        buffer_.append(json_value);
    }
    
    /**
     * @brief Get the encoded data as a byte string
     * @return Serialized JSON array as string
     */
    std::string Bytes() const {
        Json::StreamWriterBuilder writer_builder;
        writer_builder["indentation"] = "";  // Compact format
        return Json::writeString(writer_builder, buffer_);
    }
    
    /**
     * @brief Clear the internal buffer
     */
    void Clear() {
        buffer_.clear();
        buffer_ = Json::Value(Json::arrayValue);
    }
    
private:
    Encoder() : buffer_(Json::arrayValue) {}
    
    Json::Value buffer_;  // Internal JSON array buffer
};

/**
 * @brief Go-style Decoder for sequential decoding of multiple values
 * 
 * Usage:
 *   auto decoder = Decoder::New(data);
 *   int term;
 *   int votedFor;
 *   std::vector<LogEntry> logs;
 *   decoder->Decode(term);
 *   decoder->Decode(votedFor);
 *   decoder->Decode(logs);
 */
class Decoder {
public:
    using ptr = std::shared_ptr<Decoder>;
    
    /**
     * @brief Create a new decoder instance from byte data
     * @param data Serialized JSON array as string
     */
    static ptr New(const std::string& data) {
        return std::shared_ptr<Decoder>(new Decoder(data));
    }
    
    /**
     * @brief Decode the next value from buffer
     * @tparam T Type of value to decode (must be supported by Serializer)
     * @param value Reference to store decoded value
     * @return true if successful, false if no more data or parse error
     */
    template<typename T>
    bool Decode(T& value) {
        if (index_ >= buffer_.size()) {
            return false;  // No more data
        }
        
        try {
            value = Serializer<T>::deserialize(buffer_[static_cast<Json::ArrayIndex>(index_)]);
            ++index_;
            return true;
        } catch (const std::exception& e) {
            // Log error but don't throw
            return false;
        }
    }
    
    /**
     * @brief Check if there's more data to decode
     */
    bool HasMore() const {
        return index_ < buffer_.size();
    }
    
    /**
     * @brief Get current decode position
     */
    size_t Position() const {
        return index_;
    }
    
    /**
     * @brief Reset decoder to beginning
     */
    void Reset() {
        index_ = 0;
    }
    
private:
    explicit Decoder(const std::string& data) : index_(0) {
        Json::CharReaderBuilder reader_builder;
        std::string errs;
        std::istringstream iss(data);
        
        if (!Json::parseFromStream(reader_builder, iss, &buffer_, &errs)) {
            // Parse failed, create empty array
            buffer_ = Json::Value(Json::arrayValue);
        }
        
        if (!buffer_.isArray()) {
            // Not an array, wrap in array or create empty
            buffer_ = Json::Value(Json::arrayValue);
        }
    }
    
    Json::Value buffer_;  // Internal JSON array buffer
    size_t index_;        // Current decode position
};

} // namespace rpc

#endif // RPC_ENCODER_H
