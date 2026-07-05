#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>

// HTTP响应类
class HttpResponse {
public:
    // 状态码
    enum class StatusCode {
        OK = 200,
        CREATED = 201,
        NO_CONTENT = 204,
        MOVED_PERMANENTLY = 301,
        FOUND = 302,
        NOT_MODIFIED = 304,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        INTERNAL_SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        SERVICE_UNAVAILABLE = 503
    };

    // 构造函数
    HttpResponse();

    // 重置
    void reset();

    // 设置状态码
    void set_status(StatusCode code) { status_code_ = code; }

    // 设置状态码和消息
    void set_status(int code, const std::string& message);

    // 设置Content-Type
    void set_content_type(const std::string& content_type) {
        content_type_ = content_type;
    }

    // 设置Content-Length
    void set_content_length(size_t length) {
        content_length_ = length;
    }

    // 设置响应体
    void set_body(const std::string& body) {
        body_ = body;
        content_length_ = body.length();
    }

    // 添加响应头
    void add_header(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    // 设置文件响应
    void set_file_response(const std::string& file_path,
                           const std::string& content_type,
                           size_t file_size);

    // 设置错误响应
    void set_error_response(StatusCode code, const std::string& message);

    // 序列化响应为字符串
    std::string to_string() const;

    // 获取状态码
    StatusCode get_status() const { return status_code_; }

    // 获取状态消息
    const std::string& get_status_message() const { return status_message_; }

    // 获取响应体
    const std::string& get_body() const { return body_; }

    // 获取Content-Type
    const std::string& get_content_type() const { return content_type_; }

    // 获取Content-Length
    size_t get_content_length() const { return content_length_; }

    // 静态方法：获取MIME类型
    static std::string get_mime_type(const std::string& file_path);

    // 静态方法：获取状态码对应的消息
    static std::string get_status_message(int code);

private:
    StatusCode status_code_;
    std::string status_message_;
    std::string content_type_;
    size_t content_length_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};

#endif // HTTP_RESPONSE_H