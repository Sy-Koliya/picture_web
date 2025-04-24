cmake_minimum_required(VERSION 3.23)
project(async_server)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(MYSQL_LIBRARIES
  NAMES mysqlclient
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)
if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARIES)
  message(FATAL_ERROR "未找到 MySQL 客户端开发包 (libmysqlclient-dev)，请安装后重试。")
endif()
find_package(fmt REQUIRED)

# 引入子模块：fmt 和 dbimpl
add_subdirectory(fmt)
add_subdirectory(dbimpl)

# 生成可执行文件
add_executable(async_server
    async_server.cc
)

# 包含 MySQL 头文件路径
target_include_directories(async_server PRIVATE
    ${MySQL_INCLUDE_DIR}
)

# 链接库：dbimpl、fmt 和 MySQL 客户端
target_link_libraries(async_server PRIVATE
    dbimpl
    fmt::fmt
    ${MySQL_LIBRARIES}
)
