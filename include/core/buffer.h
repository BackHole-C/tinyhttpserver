#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

class Buffer {
public:
    // 构造函数
    Buffer(size_t initial_capacity = 8192);

    // 析构函数
    ~Buffer() = default;

    // 禁止拷贝，允许移动
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    // 可读字节数
    size_t readable_bytes() const { return write_pos_ - read_pos_; }

    // 可写字节数
    size_t writable_bytes() const { return buffer_.size() - write_pos_; }

    // 获取读取位置
    const char* peek() const { return buffer_.data() + read_pos_; }

    // 追加数据
    void append(const char* data, size_t len);
    void append(const std::string& str);
    void append(const void* data, size_t len);

    // 确保有足够的可写空间
    void ensure_writable_bytes(size_t len);

    // 已读走n个字节
    void retrieve(size_t n);
    void retrieve_all();

    // 查找指定字符串
    int find(const std::string& str) const;
    int find(const char* str) const;

    // 获取FD（用于调试）
    int get_fd() const { return fd_; }
    void set_fd(int fd) { fd_ = fd; }

private:
    std::vector<char> buffer_;
    size_t read_pos_;   // 读位置
    size_t write_pos_;  // 写位置
    int fd_;            // 关联的fd（用于调试）
};

#endif // BUFFER_H