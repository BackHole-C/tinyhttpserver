#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <sys/epoll.h>
#include <chrono>
#include "http/connection.h"
#include "utils/thread_pool.h"
#include "cache/file_cache.h"
#include "cache/redis_cache.h"
#include "db/database.h"

class HttpServer
{
public:
    HttpServer(int port = 8080, const std::string& root_dir = "./frontend");

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer) = delete;

    HttpServer(HttpServer&& other) noexcept;
    HttpServer& operator=(HttpServer&& other) noexcept;

    ~HttpServer();

    void start();
    void stop();

    bool is_running() const { return is_running_; }
    int get_port() const { return port_; }
    const std::string& get_root_dir() const { return root_dir_; }

    size_t get_connection_count() const;

    void set_connection_timeout(int seconds) { connection_timeout_ = seconds; }
    void set_request_timeout(int seconds) { request_timeout_ = seconds; }

private:
    int port_;
    std::string root_dir_;
    int server_fd_;
    int epoll_fd_;
    bool is_running_;

    static const int MAX_EVENTS = 1024;

    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<FileCache> file_cache_;
    std::unique_ptr<RedisCache> redis_cache_;
    std::unique_ptr<Database> database_;

    int connection_timeout_;
    int request_timeout_;

    void initialize_socket();
    void initialize_epoll();
    void initialize_thread_pool();
    void initialize_file_cache();
    void initialize_redis_cache();
    void initialize_database();

    FileCache* get_file_cache() const { return file_cache_.get(); }
    RedisCache* get_redis_cache() const { return redis_cache_.get(); }
    Database* get_database() const { return database_.get(); }

    void set_nonblocking(int fd);
    void add_epoll_event(int fd, uint32_t events);
    void modify_epoll_event(int fd, uint32_t events);
    void remove_epoll_event(int fd);
    void cleanup();
    void run_epoll_loop();
    void accept_connection();
    void close_connection(int fd);
    void check_timeouts();

    friend class Connection;
};

#endif