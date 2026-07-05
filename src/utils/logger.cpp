#include "utils/logger.h"
#include <fstream>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : level_(LogLevel::INFO),
      file_output_(false) {
}

void Logger::set_file_output(bool enable, const std::string& filename) {
    file_output_ = enable;
    filename_ = filename;
}

std::string Logger::get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO ";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

std::string Logger::format(LogLevel level, const std::string& message) {
    std::stringstream ss;
    ss << "[" << get_current_time() << "] "
       << "[" << level_to_string(level) << "] "
       << message;
    return ss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_) {
        return;
    }

    std::string formatted = format(level, message);

    // 输出到控制台
    if (level >= LogLevel::ERROR) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }

    // 输出到文件
    if (file_output_ && !filename_.empty()) {
        std::ofstream file(filename_, std::ios::app);
        if (file.is_open()) {
            file << formatted << std::endl;
            file.close();
        }
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}