#include "http/http_server.h"
#include "utils/logger.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdexcept>

HttpServer::HttpServer(int port, const std::string& root_dir)
    : port_(port),
      root_dir_(root_dir),
      server_fd_(-1),
      epoll_fd_(-1),
      is_running_(false),
      connection_timeout_(5),
      request_timeout_(10) {

    Logger::instance().info("Creating HttpServer (port: " + std::to_string(port_) +
                           ", root_dir: " + root_dir_ + ")");
}

HttpServer::HttpServer(HttpServer&& other) noexcept
    : port_(other.port_),
      root_dir_(std::move(other.root_dir_)),
      server_fd_(other.server_fd_),
      epoll_fd_(other.epoll_fd_),
      is_running_(other.is_running_),
      connections_(std::move(other.connections_)),
      thread_pool_(std::move(other.thread_pool_)),
      connection_timeout_(other.connection_timeout_),
      request_timeout_(other.request_timeout_) {
    other.server_fd_ = -1;
    other.epoll_fd_ = -1;
    other.is_running_ = false;
}

HttpServer& HttpServer::operator=(HttpServer&& other) noexcept {
    if (this != &other) {
        cleanup();

        port_ = other.port_;
        root_dir_ = std::move(other.root_dir_);
        server_fd_ = other.server_fd_;
        epoll_fd_ = other.epoll_fd_;
        is_running_ = other.is_running_;
        connections_ = std::move(other.connections_);
        thread_pool_ = std::move(other.thread_pool_);
        connection_timeout_ = other.connection_timeout_;
        request_timeout_ = other.request_timeout_;

        other.server_fd_ = -1;
        other.epoll_fd_ = -1;
        other.is_running_ = false;
    }
    return *this;
}

HttpServer::~HttpServer() {
    cleanup();
    Logger::instance().info("HttpServer destroyed");
}

void HttpServer::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl F_GETFL failed: " + std::string(strerror(errno)));
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed: " + std::string(strerror(errno)));
    }
}

void HttpServer::add_epoll_event(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        throw std::runtime_error("epoll_ctl ADD failed: " + std::string(strerror(errno)));
    }
}

void HttpServer::modify_epoll_event(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        throw std::runtime_error("epoll_ctl MOD failed: " + std::string(strerror(errno)));
    }
}

void HttpServer::remove_epoll_event(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

void HttpServer::initialize_socket() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("socket creation failed: " + std::string(strerror(errno)));
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("setsockopt failed: " + std::string(strerror(errno)));
    }

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd_);
        throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
    }

    if (listen(server_fd_, SOMAXCONN) < 0) {
        close(server_fd_);
        throw std::runtime_error("listen failed: " + std::string(strerror(errno)));
    }

    set_nonblocking(server_fd_);

    Logger::instance().info("Server listening on port " + std::to_string(port_));
}

void HttpServer::initialize_epoll() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        throw std::runtime_error("epoll_create1 failed: " + std::string(strerror(errno)));
    }

    add_epoll_event(server_fd_, EPOLLIN | EPOLLET);

    Logger::instance().info("epoll initialized");
}

void HttpServer::initialize_thread_pool() {
    thread_pool_ = std::make_unique<ThreadPool>();
    Logger::instance().info("ThreadPool initialized with " + 
                           std::to_string(thread_pool_->get_pool_size()) + " threads");
}

void HttpServer::initialize_file_cache() {
    file_cache_ = std::make_unique<FileCache>();
    Logger::instance().info("FileCache initialized (max_size: 100MB, ttl: 300s)");
}

void HttpServer::initialize_database() {
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    std::string db_path;
    
    if (len != -1) {
        exe_path[len] = '\0';
        std::string exe_str(exe_path);
        size_t last_slash = exe_str.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string bin_dir = exe_str.substr(0, last_slash);
            size_t second_last_slash = bin_dir.find_last_of('/');
            if (second_last_slash != std::string::npos) {
                std::string build_dir = bin_dir.substr(0, second_last_slash);
                size_t third_last_slash = build_dir.find_last_of('/');
                if (third_last_slash != std::string::npos) {
                    db_path = build_dir.substr(0, third_last_slash) + "/tinyhttp.db";
                } else {
                    db_path = build_dir + "/../tinyhttp.db";
                }
            } else {
                db_path = "./tinyhttp.db";
            }
        } else {
            db_path = "./tinyhttp.db";
        }
    } else {
        db_path = "./tinyhttp.db";
    }
    
    database_ = std::make_unique<Database>(db_path);
    if (database_->init()) {
        Logger::instance().info("Database initialized, message count: " + 
                               std::to_string(database_->get_message_count()));
    } else {
        Logger::instance().error("Failed to initialize database");
    }
}

void HttpServer::start() {
    if (is_running_) {
        Logger::instance().warning("Server is already running");
        return;
    }

    try {
        initialize_socket();
        initialize_epoll();
        initialize_thread_pool();
        initialize_file_cache();
        initialize_database();
        is_running_ = true;

        Logger::instance().info("Server started successfully");
        run_epoll_loop();

    } catch (const std::exception& e) {
        cleanup();
        throw;
    }
}

void HttpServer::stop() {
    if (!is_running_) {
        return;
    }

    Logger::instance().info("Stopping server...");
    is_running_ = false;

    if (server_fd_ != -1) {
        shutdown(server_fd_, SHUT_RDWR);
    }
}

void HttpServer::accept_connection() {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            Logger::instance().error("accept failed: " + std::string(strerror(errno)));
            break;
        }

        set_nonblocking(client_fd);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        Logger::instance().info("New connection: " + std::string(client_ip) + ":" +
                               std::to_string(client_port) +
                               " (fd=" + std::to_string(client_fd) + ")");

        Connection* conn = new Connection(client_fd, this);
        connections_[client_fd] = std::unique_ptr<Connection>(conn);

        add_epoll_event(client_fd, EPOLLIN | EPOLLET | EPOLLRDHUP);
    }
}

void HttpServer::close_connection(int fd) {
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        remove_epoll_event(fd);
        connections_.erase(it);

        Logger::instance().info("Connection closed (fd=" + std::to_string(fd) + ")");
    }
}

void HttpServer::check_timeouts() {
    auto now = std::chrono::steady_clock::now();

    for (auto it = connections_.begin(); it != connections_.end(); ) {
        Connection* conn = it->second.get();
        auto last_active = conn->get_last_active_time();
        
        std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_active);

        if (elapsed.count() > connection_timeout_) {
            Logger::instance().warning("Connection timeout (fd=" + 
                                     std::to_string(it->first) + 
                                     "), elapsed: " + std::to_string(elapsed.count()) + "s");
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

void HttpServer::run_epoll_loop() {
    struct epoll_event events[MAX_EVENTS];
    int timeout_count = 0;

    while (is_running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            Logger::instance().error("epoll_wait failed: " + std::string(strerror(errno)));
            break;
        }

        if (++timeout_count >= 2) {
            check_timeouts();
            timeout_count = 0;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd_) {
                accept_connection();
            } else {
                auto it = connections_.find(fd);
                if (it == connections_.end()) {
                    continue;
                }

                Connection* conn = it->second.get();

                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    Logger::instance().info("Connection error (fd=" + std::to_string(fd) + ")");
                    close_connection(fd);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    conn->update_last_active_time();
                    conn->handle_read();

                    if (conn->get_state() == ConnectionState::CLOSING) {
                        close_connection(fd);
                        continue;
                    }

                    if (conn->get_state() == ConnectionState::WRITING) {
                        modify_epoll_event(fd, EPOLLOUT | EPOLLET | EPOLLRDHUP);
                    }
                }

                if (events[i].events & EPOLLOUT) {
                    conn->update_last_active_time();
                    conn->handle_write();

                    if (conn->get_state() == ConnectionState::CLOSING) {
                        close_connection(fd);
                    }
                }
            }
        }
    }
}

void HttpServer::cleanup() {
    connections_.clear();

    thread_pool_.reset();

    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }

    if (server_fd_ != -1) {
        close(server_fd_);
        server_fd_ = -1;
    }

    is_running_ = false;
}

size_t HttpServer::get_connection_count() const {
    return connections_.size();
}