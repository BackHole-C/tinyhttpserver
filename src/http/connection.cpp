#include "http/connection.h"
#include "http/http_server.h"
#include "utils/logger.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <stdexcept>

Connection::Connection(int fd, HttpServer* server)
    : fd_(fd),
      state_(ConnectionState::READING),
      server_(server),
      read_buffer_(8192),
      write_buffer_(65536),
      last_active_time_(std::chrono::steady_clock::now()) {
    read_buffer_.set_fd(fd_);
    Logger::instance().info("Connection created (fd=" + std::to_string(fd_) + ")");
}

Connection::~Connection() {
    close();
}

void Connection::close() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

std::string Connection::get_root_dir() const {
    if (server_) {
        return server_->get_root_dir();
    }
    return "./static";
}

FileCache* Connection::get_file_cache() const {
    if (server_) {
        return server_->file_cache_.get();
    }
    return nullptr;
}

Database* Connection::get_database() const {
    if (server_) {
        return server_->database_.get();
    }
    return nullptr;
}

std::string url_decode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            try {
                int value = std::stoi(str.substr(i + 1, 2), nullptr, 16);
                result += static_cast<char>(value);
                i += 2;
            } catch (const std::exception&) {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

void Connection::handle_read() {
    char buffer[4096];

    while (true) {
        int bytes_read = recv(fd_, buffer, sizeof(buffer), 0);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            Logger::instance().error("recv failed: " + std::string(strerror(errno)));
            state_ = ConnectionState::CLOSING;
            return;
        } else if (bytes_read == 0) {
            Logger::instance().info("Client closed connection (fd=" + std::to_string(fd_) + ")");
            state_ = ConnectionState::CLOSING;
            return;
        }

        read_buffer_.append(buffer, bytes_read);
    }

    if (!request_.is_complete()) {
        const char* request_data = read_buffer_.peek();
        size_t request_len = read_buffer_.readable_bytes();

        if (request_.parse(request_data, request_len)) {
            Logger::instance().info("Request: " + request_.get_method_string() +
                                    " " + request_.get_path());

            process_request();

            read_buffer_.retrieve_all();
            request_.reset();
        }
    }
}

void Connection::handle_write() {
    while (write_buffer_.readable_bytes() > 0) {
        int bytes_written = send(fd_,
                                  write_buffer_.peek(),
                                  write_buffer_.readable_bytes(),
                                  0);

        if (bytes_written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 缓冲区满，等待下次可写事件
                return;
            }
            Logger::instance().error("send failed: " + std::string(strerror(errno)));
            state_ = ConnectionState::CLOSING;
            return;
        }

        write_buffer_.retrieve(bytes_written);
    }

    // 发送完成，关闭连接
    Logger::instance().info("Response sent complete (fd=" + std::to_string(fd_) + ")");
    state_ = ConnectionState::CLOSING;
}

void Connection::process_request() {
    Logger::instance().info("Processing request: " + request_.get_method_string() +
                            " " + request_.get_path());

    // 处理回显请求（移除查询参数）
    std::string path = request_.get_path();
    size_t query_pos = path.find('?');
    std::string clean_path = (query_pos != std::string::npos) ? path.substr(0, query_pos) : path;
    
    if (clean_path == "/echo") {
        handle_echo_request();
        return;
    }

    // 处理 API 请求
    if (clean_path == "/api/messages") {
        handle_api_messages();
        return;
    }

    // 只处理GET请求
    if (request_.get_method() != HttpRequest::Method::GET) {
        Logger::instance().warning("Method not implemented: " + request_.get_method_string());
        response_.set_error_response(HttpResponse::StatusCode::NOT_IMPLEMENTED,
                                     "Not Implemented");
        state_ = ConnectionState::WRITING;
        return;
    }

    // 处理路径（使用之前清理过的路径）
    if (clean_path == "/") {
        clean_path = "/index.html";
    }
    Logger::instance().info("Final path: " + clean_path);

    // 安全检查：防止路径遍历攻击
    if (clean_path.find("..") != std::string::npos) {
        Logger::instance().warning("Path traversal attempt detected");
        response_.set_error_response(HttpResponse::StatusCode::FORBIDDEN,
                                     "Forbidden");
        state_ = ConnectionState::WRITING;
        return;
    }

    // 构建本地文件路径
    std::string file_path = get_root_dir() + clean_path;
    Logger::instance().info("Full file path: " + file_path);

    std::string content;
    std::string content_type;
    bool cache_hit = false;

    FileCache* cache = get_file_cache();
    if (cache) {
        cache_hit = cache->get(file_path, content, content_type);
        if (cache_hit) {
            Logger::instance().info("Cache hit: " + file_path +
                                   ", hit_rate: " + std::to_string(cache->hit_rate() * 100) + "%");
        }
    }

    if (!cache_hit) {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);

        if (!file.is_open()) {
            Logger::instance().warning("File not found: " + file_path);
            response_.set_error_response(HttpResponse::StatusCode::NOT_FOUND,
                                         "Not Found");
            state_ = ConnectionState::WRITING;
            return;
        }

        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> file_data(file_size);
        if (!file.read(file_data.data(), file_size)) {
            response_.set_error_response(HttpResponse::StatusCode::INTERNAL_SERVER_ERROR,
                                         "Internal Server Error");
            state_ = ConnectionState::WRITING;
            return;
        }

        content = std::string(file_data.begin(), file_data.end());
        content_type = HttpResponse::get_mime_type(file_path);

        if (cache) {
            cache->put(file_path, content, content_type);
            Logger::instance().info("Cache miss: " + file_path +
                                   ", cached (" + std::to_string(content.size()) + " bytes)");
        }
    }

    response_.set_status(static_cast<int>(HttpResponse::StatusCode::OK), "OK");
    response_.set_content_type(content_type);
    response_.set_content_length(content.size());
    response_.set_body(content);

    std::string response_str = response_.to_string();
    write_buffer_.append(response_str.data(), response_str.size());

    state_ = ConnectionState::WRITING;

    Logger::instance().info("Serving file: " + file_path +
                            " (" + std::to_string(content.size()) + " bytes)" +
                            ", response size: " + std::to_string(response_str.size()) +
                            ", cache: " + (cache_hit ? "HIT" : "MISS"));
}

void Connection::handle_echo_request() {
    std::string message;
    
    if (request_.get_method() == HttpRequest::Method::POST) {
        message = request_.get_body();
    } else {
        // GET请求，从查询参数中获取message
        std::string path = request_.get_path();
        size_t query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            std::string query = path.substr(query_pos + 1);
            size_t msg_pos = query.find("message=");
            if (msg_pos != std::string::npos) {
                message = query.substr(msg_pos + 8);
            }
        }
    }
    
    if (message.empty()) {
        message = "Hello from TinyHttpServer!";
    }
    
    Logger::instance().info("Echo request received: \"" + message + "\"");
    
    response_.set_status(static_cast<int>(HttpResponse::StatusCode::OK), "OK");
    response_.set_content_type("text/plain");
    response_.set_content_length(message.size());
    response_.set_body(message);
    
    std::string response_str = response_.to_string();
    write_buffer_.append(response_str.data(), response_str.size());
    
    state_ = ConnectionState::WRITING;
    
    Logger::instance().info("Echo response ready, size: " + std::to_string(response_str.size()));
}

void Connection::handle_api_messages() {
    Database* db = get_database();
    if (!db) {
        response_.set_error_response(HttpResponse::StatusCode::INTERNAL_SERVER_ERROR,
                                     "Database not available");
        std::string response_str = response_.to_string();
        write_buffer_.append(response_str.data(), response_str.size());
        state_ = ConnectionState::WRITING;
        return;
    }

    if (request_.get_method() == HttpRequest::Method::GET) {
        std::vector<Message> messages;
        if (db->get_messages(messages)) {
            std::string json = "[\n";
            for (size_t i = 0; i < messages.size(); ++i) {
                const Message& msg = messages[i];
                json += "  {\n";
                json += "    \"id\": " + std::to_string(msg.id) + ",\n";
                json += "    \"name\": \"" + msg.name + "\",\n";
                json += "    \"email\": \"" + msg.email + "\",\n";
                json += "    \"content\": \"" + msg.content + "\",\n";
                json += "    \"created_at\": \"" + msg.created_at + "\"\n";
                json += "  }";
                if (i < messages.size() - 1) {
                    json += ",";
                }
                json += "\n";
            }
            json += "]";

            response_.set_status(static_cast<int>(HttpResponse::StatusCode::OK), "OK");
            response_.set_content_type("application/json; charset=utf-8");
            response_.set_content_length(json.size());
            response_.set_body(json);
            Logger::instance().info("API: GET /api/messages, count: " + std::to_string(messages.size()));
        } else {
            response_.set_error_response(HttpResponse::StatusCode::INTERNAL_SERVER_ERROR,
                                         "Failed to query messages");
        }
    } else if (request_.get_method() == HttpRequest::Method::POST) {
        std::string body = request_.get_body();
        size_t name_pos = body.find("name=");
        size_t email_pos = body.find("email=");
        size_t message_pos = body.find("message=");

        if (name_pos == std::string::npos || email_pos == std::string::npos || 
            message_pos == std::string::npos) {
            response_.set_error_response(HttpResponse::StatusCode::BAD_REQUEST,
                                         "Missing parameters");
            std::string response_str = response_.to_string();
            write_buffer_.append(response_str.data(), response_str.size());
            state_ = ConnectionState::WRITING;
            return;
        }

        std::string name = body.substr(name_pos + 5);
        size_t name_end = name.find('&');
        if (name_end != std::string::npos) name = name.substr(0, name_end);
        name = url_decode(name);

        std::string email = body.substr(email_pos + 6);
        size_t email_end = email.find('&');
        if (email_end != std::string::npos) email = email.substr(0, email_end);
        email = url_decode(email);

        std::string content = body.substr(message_pos + 8);
        content = url_decode(content);

        if (content.size() > 200) {
            std::string response_body = "{\"success\": false, \"error\": \"留言字数不能超过200字\"}";
            response_.set_status(static_cast<int>(HttpResponse::StatusCode::BAD_REQUEST), "Bad Request");
            response_.set_content_type("application/json; charset=utf-8");
            response_.set_content_length(response_body.size());
            response_.set_body(response_body);
            std::string response_str = response_.to_string();
            write_buffer_.append(response_str.data(), response_str.size());
            state_ = ConnectionState::WRITING;
            return;
        }

        if (db->add_message(name, email, content)) {
            std::string response_body = "{\"success\": true}";
            response_.set_status(static_cast<int>(HttpResponse::StatusCode::OK), "OK");
            response_.set_content_type("application/json; charset=utf-8");
            response_.set_content_length(response_body.size());
            response_.set_body(response_body);
            Logger::instance().info("API: POST /api/messages, name: " + name);
        } else {
            response_.set_error_response(HttpResponse::StatusCode::INTERNAL_SERVER_ERROR,
                                         "Failed to save message");
        }
    } else {
        response_.set_error_response(HttpResponse::StatusCode::NOT_IMPLEMENTED,
                                     "Method not implemented");
    }

    std::string response_str = response_.to_string();
    write_buffer_.append(response_str.data(), response_str.size());
    state_ = ConnectionState::WRITING;
}