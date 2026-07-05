#include "core/buffer.h"

Buffer::Buffer(size_t initial_capacity)
    : buffer_(initial_capacity),
      read_pos_(0),
      write_pos_(0),
      fd_(-1) {
}

// 移动构造函数
Buffer::Buffer(Buffer&& other) noexcept
    : buffer_(std::move(other.buffer_)),
      read_pos_(other.read_pos_),
      write_pos_(other.write_pos_),
      fd_(other.fd_) {
    other.read_pos_ = 0;
    other.write_pos_ = 0;
    other.fd_ = -1;
}

// 移动赋值运算符
Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        buffer_ = std::move(other.buffer_);
        read_pos_ = other.read_pos_;
        write_pos_ = other.write_pos_;
        fd_ = other.fd_;

        other.read_pos_ = 0;
        other.write_pos_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

void Buffer::ensure_writable_bytes(size_t len) {
    // 如果剩余空间足够，直接返回
    if (writable_bytes() >= len) {
        return;
    }

    // 如果读取位置后面的空间足够，回收已读数据
    if (buffer_.size() - write_pos_ + read_pos_ >= len) {
        size_t readable = readable_bytes();
        std::memmove(buffer_.data(),
                     buffer_.data() + read_pos_,
                     readable);
        read_pos_ = 0;
        write_pos_ = readable;
    } else {
        // 需要扩容
        size_t new_capacity = std::max(buffer_.size() * 2, write_pos_ + len);
        buffer_.resize(new_capacity);
    }
}

void Buffer::append(const char* data, size_t len) {
    ensure_writable_bytes(len);
    std::memcpy(buffer_.data() + write_pos_, data, len);
    write_pos_ += len;
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.size());
}

void Buffer::append(const void* data, size_t len) {
    append(static_cast<const char*>(data), len);
}

void Buffer::retrieve(size_t n) {
    if (n >= readable_bytes()) {
        retrieve_all();
    } else {
        read_pos_ += n;
    }
}

void Buffer::retrieve_all() {
    read_pos_ = 0;
    write_pos_ = 0;
}

int Buffer::find(const std::string& str) const {
    return find(str.c_str());
}

int Buffer::find(const char* str) const {
    if (readable_bytes() == 0) {
        return -1;
    }

    const char* start = peek();
    const char* found = std::search(start,
                                     start + readable_bytes(),
                                     str,
                                     str + strlen(str));

    if (found == start + readable_bytes()) {
        return -1;
    }

    return found - start;
}