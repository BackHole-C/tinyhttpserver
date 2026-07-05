cmake_minimum_required(VERSION 3.10)
project(TinyHttpServer VERSION 2.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译警告选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 源文件列表
set(SERVER_SOURCES
    src/http/http_server.cpp
    src/http/connection.cpp
    src/core/buffer.cpp
    src/utils/logger.cpp
    src/http/http_request.cpp
    src/http/http_response.cpp
    src/utils/thread_pool.cpp
    src/cache/file_cache.cpp
    src/db/database.cpp
)

# 链接 SQLite3 库
find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})

# 可执行文件
add_executable(server
    src/main.cpp
    ${SERVER_SOURCES}
)

# 设置输出目录
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

target_link_libraries(server ${SQLite3_LIBRARIES})