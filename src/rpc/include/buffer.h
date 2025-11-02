#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <cstring>

namespace rpc {

// 累积缓冲区，用于处理TCP粘包
class Buffer {
public:
    Buffer() : buffer_(), read_index_(0) {}
    
    // 追加数据
    void append(const char* data, size_t len) {
        buffer_.insert(buffer_.end(), data, data + len);
    }
    
    void append(const std::string& str) {
        append(str.data(), str.size());
    }
    
    // 获取可读数据大小
    size_t readable() const {
        return buffer_.size() - read_index_;
    }
    
    // 查看数据（不消费）
    const char* peek() const {
        return buffer_.data() + read_index_;
    }
    
    // 消费指定长度的数据
    void consume(size_t len) {
        if (len > readable()) {
            len = readable();
        }
        read_index_ += len;
        
        // 当已读数据超过一半时，压缩buffer
        if (read_index_ > buffer_.size() / 2) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + read_index_);
            read_index_ = 0;
        }
    }
    
    // 提取指定长度的数据（返回并消费）
    std::string retrieve(size_t len) {
        if (len > readable()) {
            len = readable();
        }
        std::string result(peek(), len);
        consume(len);
        return result;
    }
    
    // 清空buffer
    void clear() {
        buffer_.clear();
        read_index_ = 0;
    }
    
    // 读取固定长度的数据（用于读取uint32_t等）
    template<typename T>
    bool readFixedSize(T& value) {
        if (readable() < sizeof(T)) {
            return false;
        }
        std::memcpy(&value, peek(), sizeof(T));
        consume(sizeof(T));
        return true;
    }

private:
    std::vector<char> buffer_;
    size_t read_index_;  // 已读位置
};

} // namespace rpc
