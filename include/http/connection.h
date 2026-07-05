#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include "core/buffer.h"
#include "http/http_request.h"
#include "http/http_response.h"
#include "cache/file_cache.h"
#include "db/database.h"

enum class ConnectionState {
    READING,    // 正在读取请求
    WRITING,    // 正在发送响应
    CLOSING     // 准备关闭
};

class HttpServer;

class Connection {
public:
    Connection(int fd, HttpServer* server);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int get_fd() const { return fd_; }
    ConnectionState get_state() const { return state_; }
    void set_state(ConnectionState state) { state_ = state; }

    Buffer* get_read_buffer() { return &read_buffer_; }
    Buffer* get_write_buffer() { return &write_buffer_; }

    HttpRequest* get_request() { return &request_; }
    HttpResponse* get_response() { return &response_; }

    void handle_read();
    void handle_write();
    void close();

    std::chrono::steady_clock::time_point get_last_active_time() const { return last_active_time_; }
    void update_last_active_time() { last_active_time_ = std::chrono::steady_clock::now(); }

private:
    void process_request();
    void handle_echo_request();
    void handle_api_messages();
    std::string get_root_dir() const;
    FileCache* get_file_cache() const;
    Database* get_database() const;

    int fd_;
    ConnectionState state_;
    HttpServer* server_;

    Buffer read_buffer_;
    Buffer write_buffer_;
    HttpRequest request_;
    HttpResponse response_;

    std::chrono::steady_clock::time_point last_active_time_;
};

#endif // CONNECTION_H