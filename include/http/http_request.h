#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>

// HTTP请求类
class HttpRequest {
public:
    // HTTP方法
    enum class Method {
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS,
        UNKNOWN
    };

    // 解析状态
    enum class ParseState {
        PARSING_REQUEST_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
        PARSE_COMPLETE,
        PARSE_ERROR
    };

    // 构造函数
    HttpRequest();

    // 重置
    void reset();

    // 解析请求数据
    bool parse(const char* data, size_t len);

    // 是否解析完成
    bool is_complete() const { return state_ == ParseState::PARSE_COMPLETE; }

    // 获取方法
    Method get_method() const { return method_; }

    // 获取方法字符串
    std::string get_method_string() const;

    // 获取路径
    const std::string& get_path() const { return path_; }

    // 获取协议版本
    const std::string& get_version() const { return version_; }

    // 获取请求头
    const std::string& get_header(const std::string& key) const;
    bool has_header(const std::string& key) const;

    // 获取所有请求头
    const std::unordered_map<std::string, std::string>& get_headers() const {
        return headers_;
    }

    // 获取请求体
    const std::string& get_body() const { return body_; }

    // 获取解析错误信息
    const std::string& get_error() const { return error_; }

private:
    // 解析请求行
    bool parse_request_line(const std::string& line);

    // 解析请求头
    bool parse_header(const std::string& line);

    // 解析方法字符串
    Method string_to_method(const std::string& method);

    ParseState state_;
    Method method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    size_t content_length_;
    std::string error_;
};

#endif // HTTP_REQUEST_H