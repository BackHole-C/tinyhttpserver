#include "http/http_request.h"
#include "utils/logger.h"
#include <sstream>
#include <algorithm>

HttpRequest::HttpRequest() {
    reset();
}

void HttpRequest::reset() {
    state_ = ParseState::PARSING_REQUEST_LINE;
    method_ = Method::UNKNOWN;
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
    content_length_ = 0;
    error_.clear();
}

std::string HttpRequest::get_method_string() const {
    switch (method_) {
        case Method::GET:     return "GET";
        case Method::POST:    return "POST";
        case Method::PUT:     return "PUT";
        case Method::DELETE:  return "DELETE";
        case Method::HEAD:    return "HEAD";
        case Method::OPTIONS: return "OPTIONS";
        default:              return "UNKNOWN";
    }
}

HttpRequest::Method HttpRequest::string_to_method(const std::string& method) {
    if (method == "GET") return Method::GET;
    if (method == "POST") return Method::POST;
    if (method == "PUT") return Method::PUT;
    if (method == "DELETE") return Method::DELETE;
    if (method == "HEAD") return Method::HEAD;
    if (method == "OPTIONS") return Method::OPTIONS;
    return Method::UNKNOWN;
}

bool HttpRequest::parse_request_line(const std::string& line) {
    std::istringstream stream(line);
    std::string method_str, path_str, version_str;

    if (!(stream >> method_str >> path_str >> version_str)) {
        error_ = "Invalid request line";
        state_ = ParseState::PARSE_ERROR;
        return false;
    }

    method_ = string_to_method(method_str);
    if (method_ == Method::UNKNOWN) {
        error_ = "Unknown HTTP method: " + method_str;
        state_ = ParseState::PARSE_ERROR;
        return false;
    }

    path_ = path_str;
    version_ = version_str;

    // 验证HTTP版本
    if (version_ != "HTTP/1.1" && version_ != "HTTP/1.0") {
        error_ = "Unsupported HTTP version: " + version_;
        state_ = ParseState::PARSE_ERROR;
        return false;
    }

    state_ = ParseState::PARSING_HEADERS;
    return true;
}

bool HttpRequest::parse_header(const std::string& line) {
    // 空行表示请求头结束
    if (line.empty()) {
        // 检查是否有 Content-Length 头
        auto it = headers_.find("content-length");
        if (it != headers_.end()) {
            try {
                content_length_ = std::stoi(it->second);
                if (content_length_ > 0) {
                    state_ = ParseState::PARSING_BODY;
                    return true;
                }
            } catch (...) {
                error_ = "Invalid Content-Length";
                state_ = ParseState::PARSE_ERROR;
                return false;
            }
        }
        state_ = ParseState::PARSE_COMPLETE;
        return true;
    }

    // 查找冒号位置
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        error_ = "Invalid header format";
        state_ = ParseState::PARSE_ERROR;
        return false;
    }

    // 提取键值对
    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    // 去除空白字符
    auto trim = [](std::string& str) {
        str.erase(0, str.find_first_not_of(" \t"));
        str.erase(str.find_last_not_of(" \t") + 1);
    };

    trim(key);
    trim(value);

    // 转换为小写（HTTP头不区分大小写）
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    headers_[key] = value;
    return true;
}

bool HttpRequest::parse(const char* data, size_t len) {
    std::string request(data, len);

    Logger::instance().info("Parsing HTTP request (" + std::to_string(len) + " bytes)");

    size_t pos = 0;
    std::string line;

    if (state_ == ParseState::PARSING_BODY) {
        // 继续解析请求体
        body_.append(data, len);
        if (body_.size() >= content_length_) {
            state_ = ParseState::PARSE_COMPLETE;
            return true;
        }
        return false;
    }

    while (pos < request.length()) {
        size_t line_end = request.find("\r\n", pos);

        if (line_end == std::string::npos) {
            break;
        }

        line = request.substr(pos, line_end - pos);

        switch (state_) {
            case ParseState::PARSING_REQUEST_LINE:
                if (!parse_request_line(line)) {
                    return false;
                }
                break;

            case ParseState::PARSING_HEADERS:
                if (!parse_header(line)) {
                    return false;
                }
                if (state_ == ParseState::PARSE_COMPLETE) {
                    return true;
                }
                if (state_ == ParseState::PARSING_BODY) {
                    // 检查剩余数据是否包含请求体
                    pos = line_end + 2;  // 跳过 \r\n
                    if (pos < request.length()) {
                        body_.append(request.substr(pos));
                        if (body_.size() >= content_length_) {
                            state_ = ParseState::PARSE_COMPLETE;
                            return true;
                        }
                    }
                    return false;
                }
                break;

            case ParseState::PARSE_COMPLETE:
                return true;

            default:
                return false;
        }

        pos = line_end + 2;  // 跳过 \r\n
    }

    return false;
}

const std::string& HttpRequest::get_header(const std::string& key) const {
    static const std::string empty_string;

    // 将key转换为小写进行查找
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

    auto it = headers_.find(lower_key);
    if (it != headers_.end()) {
        return it->second;
    }
    return empty_string;
}

bool HttpRequest::has_header(const std::string& key) const {
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
    return headers_.find(lower_key) != headers_.end();
}