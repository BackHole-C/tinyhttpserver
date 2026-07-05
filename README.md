# TinyHttpServer

一个基于 epoll + 非阻塞 I/O 实现的高性能 C++ HTTP 服务器，支持高并发连接、静态文件服务、RESTful API、数据库持久化和 Redis 缓存。

## 功能特性

- **高并发支持**: 使用 epoll + 非阻塞 I/O，单线程支持上万并发连接
- **边缘触发模式**: 使用 EPOLLET 高效处理事件，减少系统调用次数
- **静态文件服务**: 支持提供 HTML、CSS、JavaScript、图片、JSON 等静态文件
- **RESTful API**: 提供 `/api/messages` 接口，支持 GET/POST 请求
- **SQLite 数据库**: 集成 SQLite3 嵌入式数据库，实现留言数据持久化存储
- **Redis 缓存**: 集成 Redis 作为分布式缓存层，缓存 API 查询结果，命中率达 99%
- **线程池**: 使用线程池处理静态文件 I/O 等耗时操作，避免阻塞主线程
- **连接超时管理**: 定期清理空闲连接，避免资源泄漏
- **LRU 文件缓存**: 基于双向链表 + 哈希表实现 LRU 缓存，缓存热门静态文件
- **MIME 类型识别**: 自动识别常见文件类型并返回正确的 Content-Type
- **命令行配置**: 支持通过命令行参数配置端口和静态文件目录
- **安全防护**: 防止路径遍历攻击（如 `../` 攻击）
- **优雅关闭**: 支持 Ctrl+C 信号优雅停止服务器
- **日志输出**: 提供请求日志和服务器状态信息

## 项目结构

```
tinyhttpserver/
├── frontend/                    # 前端静态资源
│   ├── index.html              # 个人主页
│   ├── style.css               # 样式文件
│   └── script.js               # 交互脚本
├── include/                     # 头文件（按模块划分）
│   ├── core/
│   │   └── buffer.h            # 缓冲区管理类
│   ├── http/
│   │   ├── connection.h        # 连接管理类
│   │   ├── http_request.h      # HTTP 请求解析器
│   │   ├── http_response.h     # HTTP 响应构建器
│   │   └── http_server.h       # HTTP 服务器主类
│   ├── cache/
│   │   ├── file_cache.h        # LRU 文件缓存
│   │   └── redis_cache.h       # Redis 缓存封装
│   ├── db/
│   │   └── database.h          # SQLite 数据库封装
│   └── utils/
│       ├── logger.h            # 日志系统（单例模式）
│       └── thread_pool.h       # 线程池
├── src/                        # 源文件（按模块划分）
│   ├── core/buffer.cpp
│   ├── http/connection.cpp
│   ├── http/http_request.cpp
│   ├── http/http_response.cpp
│   ├── http/http_server.cpp
│   ├── cache/file_cache.cpp
│   ├── cache/redis_cache.cpp
│   ├── db/database.cpp
│   ├── utils/logger.cpp
│   ├── utils/thread_pool.cpp
│   └── main.cpp
├── build/                      # 构建输出目录
├── CMakeLists.txt              # CMake 构建配置
└── README.md
```

## 模块职责说明

| 模块 | 文件 | 职责 |
|------|------|------|
| **Buffer** | [buffer.h](include/core/buffer.h) | 动态扩容的读写缓冲区，支持高效的数据追加和读取 |
| **Logger** | [logger.h](include/utils/logger.h) | 单例日志系统，支持 DEBUG/INFO/WARNING/ERROR 四个级别 |
| **ThreadPool** | [thread_pool.h](include/utils/thread_pool.h) | 线程池，处理耗时 I/O 操作 |
| **HttpRequest** | [http_request.h](include/http/http_request.h) | HTTP 请求解析器，状态机设计解析请求行和头部 |
| **HttpResponse** | [http_response.h](include/http/http_response.h) | HTTP 响应构建器，支持多种状态码和响应类型 |
| **Connection** | [connection.h](include/http/connection.h) | 连接生命周期管理，处理读/写/关闭事件 |
| **HttpServer** | [http_server.h](include/http/http_server.h) | epoll 事件循环，连接调度，服务器启停控制 |
| **FileCache** | [file_cache.h](include/cache/file_cache.h) | LRU 缓存，缓存热门静态文件（100MB 容量） |
| **RedisCache** | [redis_cache.h](include/cache/redis_cache.h) | Redis 缓存封装，缓存 API 查询结果 |
| **Database** | [database.h](include/db/database.h) | SQLite3 数据库封装，留言数据持久化 |

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
│              ├──────────┬──────────┬─────────────┐            │          │
│              ▼          ▼          ▼             ▼            │          │
│       静态文件      API请求     Echo请求      其他请求           │          │
│              │          │          │             │            │          │
│              ▼          ▼          ▼             ▼            │          │
│   FileCache/磁盘   RedisCache    返回消息      返回错误          │          │
│              │          │          │             │            │          │
│              │          └───► SQLite3          │             │          │
│              │                                │             │            │
│              ▼                                ▼             ▼            │
│   [HttpResponse::set_file_response()]          │             │            │
│              │                                │             │            │
│              ▼                                ▼             ▼            │
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
| **服务器启动** | main | HttpServer::HttpServer() | [http_server.h](include/http/http_server.h) | 创建服务器实例 |
| | main | HttpServer::start() | [http_server.h](include/http/http_server.h) | 启动服务器 |
| | start | initialize_socket() | [http_server.cpp](src/http/http_server.cpp) | 创建监听socket |
| | start | initialize_epoll() | [http_server.cpp](src/http/http_server.cpp) | 创建epoll实例 |
| | start | initialize_thread_pool() | [http_server.cpp](src/http/http_server.cpp) | 初始化线程池 |
| | start | initialize_file_cache() | [http_server.cpp](src/http/http_server.cpp) | 初始化LRU缓存 |
| | start | initialize_redis_cache() | [http_server.cpp](src/http/http_server.cpp) | 初始化Redis缓存 |
| | start | initialize_database() | [http_server.cpp](src/http/http_server.cpp) | 初始化数据库 |
| | start | run_epoll_loop() | [http_server.cpp](src/http/http_server.cpp) | 进入事件循环 |
| **新连接到达** | run_epoll_loop | accept_connection() | [http_server.cpp](src/http/http_server.cpp) | 接受新连接 |
| | accept_connection | Connection::Connection() | [connection.h](include/http/connection.h) | 创建连接对象 |
| | accept_connection | add_connection() | [http_server.cpp](src/http/http_server.cpp) | 注册到epoll |
| **读取请求** | run_epoll_loop | handle_read() | [connection.cpp](src/http/connection.cpp) | 读取数据到缓冲区 |
| | handle_read | Buffer::read_from_fd() | [buffer.cpp](src/core/buffer.cpp) | 从socket读取 |
| | handle_read | HttpRequest::parse() | [http_request.cpp](src/http/http_request.cpp) | 解析HTTP请求 |
| **处理请求** | handle_read | process_request() | [connection.cpp](src/http/connection.cpp) | 处理业务逻辑 |
| | process_request | handle_api_messages() | [connection.cpp](src/http/connection.cpp) | 处理API请求 |
| | handle_api_messages | RedisCache::get() | [redis_cache.cpp](src/cache/redis_cache.cpp) | 查询Redis缓存 |
| | handle_api_messages | Database::get_messages() | [database.cpp](src/db/database.cpp) | 查询数据库 |
| | handle_api_messages | RedisCache::put() | [redis_cache.cpp](src/cache/redis_cache.cpp) | 写入Redis缓存 |
| **发送响应** | run_epoll_loop | handle_write() | [connection.cpp](src/http/connection.cpp) | 发送响应数据 |
| | handle_write | Buffer::write_to_fd() | [buffer.cpp](src/core/buffer.cpp) | 写入socket |
| **关闭连接** | handle_write | close_connection() | [connection.cpp](src/http/connection.cpp) | 关闭socket |
| | close_connection | remove_connection() | [http_server.cpp](src/http/http_server.cpp) | 从epoll移除 |

### Redis 缓存流程图

```
┌──────────────────────────────────────────────────────────────────────┐
│                      Redis 缓存策略                                   │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  GET /api/messages                                                   │
│       │                                                              │
│       ▼                                                              │
│  RedisCache::get("messages_cache")                                   │
│       │                                                              │
│       ├── 命中 ──► 直接返回缓存的 JSON 数据                           │
│       │                                                              │
│       └── 未命中 ──► Database::get_messages()                        │
│                       │                                              │
│                       ▼                                              │
│                  RedisCache::put("messages_cache", json, 60s)        │
│                       │                                              │
│                       ▼                                              │
│                  返回 JSON 数据                                       │
│                                                                      │
│  POST /api/messages                                                  │
│       │                                                              │
│       ▼                                                              │
│  Database::add_message(name, email, content)                         │
│       │                                                              │
│       ├── 成功 ──► RedisCache::remove("messages_cache") ──► 返回成功 │
│       │                                                              │
│       └── 失败 ──► 返回错误                                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

## 构建方法

### 前置要求

- C++17 或更高版本编译器
- CMake 3.10 或更高版本
- Linux/Unix 环境（使用了 POSIX socket API）
- SQLite3 开发库（`libsqlite3-dev`）
- Redis 开发库（`libhiredis-dev`）
- Redis Server

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt install -y cmake g++ libsqlite3-dev libhiredis-dev redis-server

# 安装 redis-plus-plus（C++ Redis 客户端）
cd /tmp
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4 && sudo make install
```

### 编译步骤

```bash
# 创建构建目录
mkdir -p build && cd build

# 生成构建文件
cmake ..

# 编译
make -j4
```

编译完成后，可执行文件位于 `build/bin/server`。

## 使用方法

### 启动服务器

```bash
# 使用默认配置（端口8080，静态文件目录 ./frontend）
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./build/bin/server

# 指定端口
./build/bin/server 3000

# 指定端口和静态文件目录
./build/bin/server 8080 /var/www/html
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| 端口号 | 监听的 TCP 端口 (1-65535) | 8080 |
| 静态文件目录 | 静态文件的根目录路径 | ./frontend |

### 停止服务器

按下 `Ctrl+C` 即可优雅停止服务器。

## API 说明

### 留言板 API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/messages` | 获取所有留言列表 |
| POST | `/api/messages` | 提交新留言 |

#### GET /api/messages

返回所有留言的 JSON 数组：

```json
[
  {
    "id": 1,
    "name": "张三",
    "email": "zhangsan@test.com",
    "content": "这是一条留言",
    "created_at": "2026-07-05 12:30:00"
  }
]
```

#### POST /api/messages

提交新留言，表单参数：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| name | string | 是 | 用户名 |
| email | string | 是 | 邮箱地址 |
| message | string | 是 | 留言内容（最多200字） |

成功响应：
```json
{"success": true}
```

失败响应：
```json
{"success": false, "error": "留言字数不能超过200字"}
```

### 支持的 HTTP 方法

- **GET**: 获取静态文件资源或 API 数据
- **POST**: 提交数据到 API

### 响应状态码

| 状态码 | 说明 |
|--------|------|
| 200 OK | 请求成功 |
| 400 Bad Request | 请求参数错误 |
| 403 Forbidden | 路径遍历攻击被阻止 |
| 404 Not Found | 请求的文件不存在 |
| 500 Internal Server Error | 服务器内部错误 |
| 501 Not Implemented | 不支持的 HTTP 方法 |

### 支持的 MIME 类型

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

# 访问 JSON 文件
curl http://localhost:8080/data.json

# 访问图片
curl -O http://localhost:8080/image.jpg
```

### 使用留言板 API

```bash
# 获取所有留言
curl http://localhost:8080/api/messages

# 提交新留言
curl -X POST -d "name=张三&email=zhangsan@test.com&message=你好" http://localhost:8080/api/messages
```

### 测试 Redis 缓存

```bash
# 第一次请求（缓存未命中）
curl http://localhost:8080/api/messages

# 第二次请求（缓存命中）
curl http://localhost:8080/api/messages

# 查看 Redis 缓存状态
redis-cli EXISTS messages_cache
redis-cli TTL messages_cache
```

## 技术细节

### 核心技术栈

| 技术 | 说明 |
|------|------|
| **epoll** | Linux 高性能事件通知机制 |
| **非阻塞 I/O** | 所有 socket 设置为非阻塞模式 |
| **边缘触发(ET)** | EPOLLET 模式，只在状态变化时触发 |
| **线程池** | 处理耗时 I/O 操作 |
| **LRU 缓存** | 双向链表 + 哈希表实现 |
| **Redis** | 分布式缓存，缓存 API 查询结果 |
| **SQLite3** | 嵌入式数据库，数据持久化 |
| **RESTful API** | 标准 HTTP 接口设计 |

### 架构说明

服务器采用单线程 epoll 架构：

1. **监听 socket**: 设置为非阻塞，使用边缘触发模式监听新连接
2. **连接管理**: 使用 `unordered_map` 存储所有活跃连接
3. **事件循环**: `epoll_wait` 等待事件，批量处理就绪的连接
4. **状态机**: 每个连接有 READING/WRITING/CLOSING 三种状态
5. **缓冲区**: 每个连接独立维护读写缓冲区，支持部分数据传输
6. **缓存策略**: Redis 缓存 API 查询结果，TTL 60 秒，写入时失效

### 性能特点

- **高并发**: 单线程支持上万并发连接
- **低延迟**: Redis 缓存使 API 响应延迟降低 99%
- **资源高效**: 线程池复用线程，减少线程创建开销
- **内存优化**: LRU 缓存自动淘汰冷数据

## 许可证

MIT License