#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

// 日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// 日志系统
class Logger {
public:
    // 获取单例实例
    static Logger& instance();

    // 设置日志级别
    void set_level(LogLevel level) { level_ = level; }

    // 设置是否输出到文件
    void set_file_output(bool enable, const std::string& filename = "");

    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 日志输出函数
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    // 格式化日志消息
    static std::string format(LogLevel level, const std::string& message);

private:
    // 私有构造函数
    Logger();

    // 获取当前时间字符串
    static std::string get_current_time();

    // 获取日志级别字符串
    static std::string level_to_string(LogLevel level);

    LogLevel level_;
    bool file_output_;
    std::string filename_;
};

#endif // LOGGER_H