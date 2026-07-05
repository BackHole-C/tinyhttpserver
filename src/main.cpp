#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <limits.h>
#include "http/http_server.h"

// 全局服务器指针，用于信号处理
HttpServer* g_server = nullptr;

void signal_handler(int signum) {
    std::cout << "\n接收到信号 " << signum << "，正在停止服务器..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

// 获取绝对路径
std::string get_absolute_path(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    // 如果已经是绝对路径，直接返回
    if (path[0] == '/') {
        return path;
    }

    // 尝试使用 realpath 解析相对路径
    char resolved_path[PATH_MAX];
    if (realpath(path.c_str(), resolved_path) != nullptr) {
        return std::string(resolved_path);
    }

    // 如果 realpath 失败，使用当前目录拼接
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::string abs_path = std::string(cwd) + "/" + path;

        // 规范化路径（移除 ./ 和多余的 /）
        size_t pos;
        while ((pos = abs_path.find("/./")) != std::string::npos) {
            abs_path.erase(pos, 2);
        }
        while ((pos = abs_path.find("//")) != std::string::npos) {
            abs_path.erase(pos, 1);
        }

        return abs_path;
    }

    return path;  // 如果失败，返回原路径
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    int port = 8080;
    std::string root_dir = "./frontend";

    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port < 1 || port > 65535) {
                std::cerr << "错误: 端口号必须在 1-65535 之间" << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "错误: 无效的端口号" << std::endl;
            return 1;
        }
    }
    if (argc > 2) {
        root_dir = get_absolute_path(argv[2]);
    } else {
        // 默认使用绝对路径
        root_dir = get_absolute_path(root_dir);
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "      TinyHttpServer v2.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "监听端口: " << port << std::endl;
    std::cout << "静态文件目录: " << root_dir << std::endl;
    std::cout << "提示: 建议从项目根目录运行，使用相对路径" << std::endl;
    std::cout << "或使用绝对路径，如: ./server 8080 /absolute/path/to/frontend" << std::endl;
    std::cout << "按 Ctrl+C 停止服务器" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // 创建服务器实例
        HttpServer server(port, root_dir);
        g_server = &server;
        
        // 注册信号处理
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        
        // 启动服务器
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "服务器已停止" << std::endl;
    return 0;
}


