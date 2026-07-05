#include "http/http_response.h"
#include "utils/logger.h"
#include <sstream>
#include <algorithm>

HttpResponse::HttpResponse() {
    reset();
}

void HttpResponse::reset() {
    status_code_ = StatusCode::OK;
    status_message_ = "OK";
    content_type_ = "text/html";
    content_length_ = 0;
    body_.clear();
    headers_.clear();
}

void HttpResponse::set_status(int code, const std::string& message) {
    status_code_ = static_cast<StatusCode>(code);
    status_message_ = message;
}

std::string HttpResponse::get_status_message(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

std::string HttpResponse::get_mime_type(const std::string& file_path) {
    // 获取文件扩展名
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = file_path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // MIME类型映射
    static const std::unordered_map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"txt", "text/plain"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"ico", "image/x-icon"},
        {"svg", "image/svg+xml"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"tar", "application/x-tar"},
        {"gz", "application/gzip"},
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"}
    };

    auto it = mime_types.find(ext);
    if (it != mime_types.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

void HttpResponse::set_file_response(const std::string& file_path,
                                     const std::string& content_type,
                                     size_t file_size) {
    status_code_ = StatusCode::OK;
    status_message_ = "OK";
    content_type_ = content_type;
    content_length_ = file_size;
    add_header("Content-Type", content_type);
    add_header("Content-Length", std::to_string(file_size));
}

void HttpResponse::set_error_response(StatusCode code, const std::string& message) {
    status_code_ = code;
    status_message_ = message;

    // 生成错误HTML页面
    std::stringstream ss;
    ss << "<!DOCTYPE html>"
       << "<html>"
       << "<head><title>" << static_cast<int>(code) << " " << message << "</title></head>"
       << "<body>"
       << "<h1>" << static_cast<int>(code) << " " << message << "</h1>"
       << "</body>"
       << "</html>";

    body_ = ss.str();
    content_type_ = "text/html";
    content_length_ = body_.length();

    add_header("Content-Type", "text/html");
    add_header("Content-Length", std::to_string(content_length_));
}

std::string HttpResponse::to_string() const {
    std::stringstream ss;

    // 状态行
    ss << "HTTP/1.1 " << static_cast<int>(status_code_) << " " << status_message_ << "\r\n";

    // 响应头
    ss << "Content-Type: " << content_type_ << "\r\n";
    ss << "Content-Length: " << content_length_ << "\r\n";

    // 额外添加的响应头
    for (const auto& header : headers_) {
        ss << header.first << ": " << header.second << "\r\n";
    }

    // 空行
    ss << "\r\n";

    // 响应体
    if (!body_.empty()) {
        ss << body_;
    }

    return ss.str();
}