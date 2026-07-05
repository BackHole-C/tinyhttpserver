# TinyHttpServer

一个轻量级的C++ HTTP静态文件服务器，基于 epoll + 非阻塞 I/O 实现高并发支持。

## 功能特性

- **高并发支持**: 使用 epoll + 非阻塞 I/O，支持成千上万并发连接
- **边缘触发模式**: 使用 EPOLLET 高效处理事件，减少系统调用次数
- **静态文件服务**: 支持提供HTML、CSS、JavaScript、图片、JSON等静态文件
- **GET请求支持**: 处理HTTP GET请求，返回静态文件内容
- **MIME类型识别**: 自动识别常见文件类型并返回正确的Content-Type
- **命令行配置**: 支持通过命令行参数配置端口和静态文件目录
- **安全防护**: 防止路径遍历攻击（如 `../` 攻击）
- **优雅关闭**: 支持 Ctrl+C 信号优雅停止服务器
- **日志输出**: 提供请求日志和服务器状态信息

## 项目结构

```
tinyhttpserver/
├── include/
│   ├── buffer.h           # 缓冲区管理类
│   ├── logger.h           # 日志系统（单例模式）
│   ├── http_request.h     # HTTP请求解析器
│   ├── http_response.h    # HTTP响应构建器
│   ├── connection.h       # 连接管理类
│   └── http_server.h      # HTTP服务器主类
├── src/
│   ├── buffer.cpp         # Buffer实现
│   ├── logger.cpp         # Logger实现
│   ├── http_request.cpp   # HttpRequest实现
│   ├── http_response.cpp  # HttpResponse实现
│   ├── connection.cpp     # Connection实现
│   └── http_server.cpp    # HttpServer实现
├── static/                # 静态文件目录
│   ├── index.html
│   ├── data.json
│   ├── image.jpg
│   └── test.txt
├── build/                 # 构建输出目录
├── CMakeLists.txt         # CMake构建配置
└── README.md
```

## 模块职责说明

| 模块 | 文件 | 职责 |
|------|------|------|
| **Buffer** | [buffer.h](include/buffer.h) | 动态扩容的读写缓冲区，支持高效的数据追加和读取 |
| **Logger** | [logger.h](include/logger.h) | 单例日志系统，支持DEBUG/INFO/WARNING/ERROR四个级别 |
| **HttpRequest** | [http_request.h](include/http_request.h) | HTTP请求解析器，状态机设计解析请求行和头部 |
| **HttpResponse** | [http_response.h](include/http_response.h) | HTTP响应构建器，支持200/403/404/500/501响应 |
| **Connection** | [connection.h](include/connection.h) | 连接生命周期管理，处理读/写/关闭事件 |
| **HttpServer** | [http_server.h](include/http_server.h) | epoll事件循环，连接调度，服务器启停控制 |

## 函数调用时机图

### 整体架构流程图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        客户端请求流程                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   客户端 ──GET /index.html──> [listen_fd]                               │
│                                     │                                   │
│                                     ▼                                   │
│                           [HttpServer::run_epoll_loop()]               │
│                                     │                                   │
│                                     ▼                                   │
│                           [HttpServer::accept_connection()]             │
│                                     │                                   │
│                                     ▼                                   │
│                           [Connection::Connection()] 创建新连接           │
│                                     │                                   │
│                                     ▼                                   │
│                           [HttpServer::add_connection()]                │
│                                     │                                   │
│                                     ▼                                   │
│                           [epoll_ctl: EPOLLIN | EPOLLET]               │
│                                     │                                   │
│              ┌───────────────────────┴───────────────────────┐          │
│              ▼                                               ▼          │
│   [Connection::handle_read()]                      [Connection::handle_write()]│
│              │                                               │          │
│              ▼                                               │          │
│   [HttpRequest::parse()]                                     │          │
│              │                                               │          │
│              ▼                                               │          │
│   [Connection::process_request()]                            │          │
│              │                                               │          │
│              ▼                                               │          │
│   [HttpResponse::set_file_response()]                        │          │
│              │                                               │          │
│              ▼                                               │          │
│   [Connection::state_ = WRITING] ────────────────────────────┘          │
│                                     │                                   │
│                                     ▼                                   │
│                           [epoll_ctl: EPOLLOUT | EPOLLET]               │
│                                     │                                   │
│                                     ▼                                   │
│                           [Connection::handle_write()]                  │
│                                     │                                   │
│                                     ▼                                   │
│                           [Connection::close_connection()]              │
│                                     │                                   │
│                                     ▼                                   │
│                           [HttpServer::remove_connection()]             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 连接状态机图

```
              ┌──────────────┐
              │   READING    │ ◄─────────────────────────────┐
              │  (读取请求)   │                               │
              └──────┬───────┘                               │
                     │                                       │
                     │ 请求读取完成                           │
                     ▼                                       │
              ┌──────────────┐                               │
              │  PROCESSING  │                               │
              │  (处理请求)   │                               │
              └──────┬───────┘                               │
                     │                                       │
                     │ 响应准备完成                           │
                     ▼                                       │
              ┌──────────────┐                               │
              │   WRITING    │─────────────────┐             │
              │  (发送响应)   │                 │             │
              └──────┬───────┘                 │             │
                     │                         │             │
                     │ 响应发送完成             │             │
                     ▼                         │             │
              ┌──────────────┐                 │             │
              │   CLOSING    │◄────────────────┘             │
              │  (关闭连接)   │                             │
              └──────┬───────┘                             │
                     │                                     │
                     │ 连接关闭完成                         │
                     ▼                                     │
              ┌──────────────┐                             │
              │   CLOSED     │─────────────────────────────┘
              │  (连接结束)   │  重新建立连接
              └──────────────┘
```

### 核心函数调用时序表

| 阶段 | 调用者 | 被调用函数 | 文件位置 | 说明 |
|------|--------|-----------|----------|------|
| **服务器启动** | main | HttpServer::HttpServer() | [http_server.h](include/http_server.h) | 创建服务器实例 |
| | main | HttpServer::start() | [http_server.h](include/http_server.h) | 启动服务器 |
| | start | HttpServer::initialize_socket() | [http_server.cpp](src/http_server.cpp) | 创建监听socket |
| | start | HttpServer::initialize_epoll() | [http_server.cpp](src/http_server.cpp) | 创建epoll实例 |
| | start | HttpServer::run_epoll_loop() | [http_server.cpp](src/http_server.cpp) | 进入事件循环 |
| **新连接到达** | run_epoll_loop | HttpServer::accept_connection() | [http_server.cpp](src/http_server.cpp) | 接受新连接 |
| | accept_connection | Connection::Connection() | [connection.h](include/connection.h) | 创建连接对象 |
| | accept_connection | HttpServer::add_connection() | [http_server.cpp](src/http_server.cpp) | 注册到epoll |
| **读取请求** | run_epoll_loop | Connection::handle_read() | [connection.cpp](src/connection.cpp) | 读取数据到缓冲区 |
| | handle_read | Buffer::read_from_fd() | [buffer.cpp](src/buffer.cpp) | 从socket读取 |
| | handle_read | HttpRequest::parse() | [http_request.cpp](src/http_request.cpp) | 解析HTTP请求 |
| **处理请求** | handle_read | Connection::process_request() | [connection.cpp](src/connection.cpp) | 处理业务逻辑 |
| | process_request | HttpResponse::set_file_response() | [http_response.cpp](src/http_response.cpp) | 构建响应 |
| **发送响应** | run_epoll_loop | Connection::handle_write() | [connection.cpp](src/connection.cpp) | 发送响应数据 |
| | handle_write | Buffer::write_to_fd() | [buffer.cpp](src/buffer.cpp) | 写入socket |
| **关闭连接** | handle_write | Connection::close_connection() | [connection.cpp](src/connection.cpp) | 关闭socket |
| | close_connection | HttpServer::remove_connection() | [http_server.cpp](src/http_server.cpp) | 从epoll移除 |

### 缓冲区数据流图

```
┌────────────────────────────────────────────────────────────────────────┐
│                         Buffer 数据流                                  │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│   socket ──read()──> [read_buffer_] ──parse()──> HttpRequest          │
│                          │                                             │
│                          ▼                                             │
│                 数据消费后自动移动                                      │
│                          │                                             │
│                          ▼                                             │
│   HttpResponse ──to_string()──> [write_buffer_] ──write()──> socket   │
│                                                                        │
│   Buffer 类特性:                                                       │
│   • 动态扩容: 超过容量时自动倍增                                         │
│   • 高效追加: 使用 write_pos_ 指针直接写入                               │
│   • 自动收缩: 读取后数据前移，保持连续性                                 │
│   • 线程安全: 单线程环境下无需额外同步                                   │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

## 构建方法

### 前置要求

- C++11 或更高版本编译器
- CMake 3.10 或更高版本
- Linux/Unix 环境（使用了POSIX socket API）

### 编译步骤

```bash
# 创建构建目录
mkdir -p build && cd build

# 生成构建文件
cmake ..

# 编译
make
```

编译完成后，可执行文件位于 `build/bin/server`。

## 使用方法

### 启动服务器

```bash
# 使用默认配置（端口8080，静态文件目录 ./static）
./build/bin/server

# 指定端口
./build/bin/server 3000

# 指定端口和静态文件目录
./build/bin/server 8080 /var/www/html
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| 端口号 | 监听的TCP端口 (1-65535) | 8080 |
| 静态文件目录 | 静态文件的根目录路径 | ./static |

### 停止服务器

按下 `Ctrl+C` 即可优雅停止服务器。

## API说明

### 支持的HTTP方法

- **GET**: 获取静态文件资源

### 响应状态码

| 状态码 | 说明 |
|--------|------|
| 200 OK | 文件请求成功 |
| 403 Forbidden | 路径遍历攻击被阻止 |
| 404 Not Found | 请求的文件不存在 |
| 500 Internal Server Error | 服务器内部错误 |
| 501 Not Implemented | 不支持的HTTP方法 |

### 支持的MIME类型

| 扩展名 | Content-Type |
|--------|--------------|
| .html, .htm | text/html |
| .css | text/css |
| .js | application/javascript |
| .json | application/json |
| .png | image/png |
| .jpg, .jpeg | image/jpeg |
| .gif | image/gif |
| .txt | text/plain |
| .pdf | application/pdf |
| 其他 | application/octet-stream |

## 示例

### 访问静态文件

```bash
# 访问首页
curl http://localhost:8080/

# 访问JSON文件
curl http://localhost:8080/data.json

# 访问图片
curl -O http://localhost:8080/image.jpg
```

## 技术细节

- **epoll事件驱动**: 使用 Linux epoll 实现高性能事件驱动架构
- **非阻塞I/O**: 所有 socket 均设置为非阻塞模式，避免阻塞整个服务器
- **边缘触发(ET)模式**: 使用 EPOLLET 模式，只在状态变化时触发事件，效率更高
- **连接状态管理**: 每个连接独立管理读写缓冲区和状态，支持部分读写
- **Socket编程**: 使用POSIX socket API实现TCP服务器
- **资源管理**: RAII风格的资源管理，确保socket和epoll正确关闭
- **移动语义**: 支持移动构造和移动赋值

### 架构说明

服务器采用单线程 epoll 架构：

1. **监听socket**: 设置为非阻塞，使用边缘触发模式监听新连接
2. **连接管理**: 使用 `unordered_map` 存储所有活跃连接
3. **事件循环**: `epoll_wait` 等待事件，批量处理就绪的连接
4. **状态机**: 每个连接有 READING/WRITING/CLOSING 三种状态
5. **缓冲区**: 每个连接独立维护读写缓冲区，支持部分数据传输

## 许可证

MIT License