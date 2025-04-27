cmake_minimum_required(VERSION 3.23)
project(async_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# 第三方
find_package(fmt        REQUIRED)
find_package(Protobuf   REQUIRED)
find_package(gRPC       REQUIRED)
find_package(MySQL      REQUIRED)
find_package(Threads    REQUIRED)

# 子模块（相对于 src/dbserver 目录）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/dbimpl   dbimpl_build)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/fmt      fmt_build)

# 源文件收集：上一级的 base 和 proto
file(GLOB_RECURSE BASE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../base/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../base/*.c"
)
file(GLOB_RECURSE PROTO_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../proto/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../proto/*.c"
)

# 本目录下的 dbapi 和 api
file(GLOB_RECURSE DBAPI_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/dbapi/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/dbapi/*.c"
)
file(GLOB_RECURSE API_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/api/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/api/*.c"
)

add_executable(server_rpc
    ${CMAKE_CURRENT_LIST_DIR}/server_rpc.cc
    ${BASE_SOURCES}
    ${DBAPI_SOURCES}
    ${API_SOURCES}
    ${PROTO_SOURCES}
)

# 包含路径：上一级根目录 + 子模块
target_include_directories(server_rpc PRIVATE
    # 项目上一级（包含 base, proto, frame 等）
    ${CMAKE_CURRENT_LIST_DIR}/..

    # 本目录及子目录
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/dbapi
    ${CMAKE_CURRENT_LIST_DIR}/api

    # 子模块头
    ${CMAKE_CURRENT_LIST_DIR}/../dbimpl
    ${CMAKE_CURRENT_LIST_DIR}/../fmt/include
    ${CMAKE_CURRENT_LIST_DIR}/../proto
    ${CMAKE_CURRENT_LIST_DIR}/../base
    ${CMAKE_CURRENT_LIST_DIR}/../frame
    # 第三方
    ${Protobuf_INCLUDE_DIRS}
    ${gRPC_INCLUDE_DIRS}
    ${MySQL_INCLUDE_DIRS}
)

target_link_libraries(server_rpc PRIVATE
    dbimpl
    fmt::fmt
    protobuf::libprotobuf
    gRPC::grpc++_unsecure 
    Threads::Threads
    ${MYSQL_LIBRARIES}
)
