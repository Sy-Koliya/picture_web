cmake_minimum_required(VERSION 3.23)
project(fdfs_server )

set(CMAKE_CXX_STANDARD       20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS     OFF)

# —— 第三方依赖 —— 
find_package(Protobuf   REQUIRED)
find_package(gRPC       REQUIRED)
find_package(Threads    REQUIRED)

find_path(REDIS_PLUS_PLUS_HEADER sw)
find_path(HIREDIS_HEADER        hiredis)
find_library(HIREDIS_LIB        hiredis)
find_library(REDIS_PLUS_PLUS_LIB redis++)


# FastDFS 客户端（libfdfsclient + libfastcommon）
find_path(FASTDFS_INCLUDE_DIR
  NAMES fdfs_client.h
  PATHS
    /usr/include
    /usr/local/include
    /usr/include/fastdfs
    /usr/local/include/fastdfs
)
find_library(FASTDFS_CLIENT_LIB
  NAMES fdfsclient
  PATHS
    /usr/lib
    /usr/local/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib64
)
find_library(FASTCOMMON_LIB
  NAMES fastcommon
  PATHS
    /usr/lib
    /usr/local/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib64
)


# —— 源码文件收集 —— 
file(GLOB BASE_SOURCES
"${CMAKE_CURRENT_LIST_DIR}/../base/*.cc"
"${CMAKE_CURRENT_LIST_DIR}/../base/*.c"
)
file(GLOB PROTO_SOURCES
"${CMAKE_CURRENT_LIST_DIR}/../proto/*.cc"
"${CMAKE_CURRENT_LIST_DIR}/../proto/*.c"
)
file(GLOB API_SOURCES
"${CMAKE_CURRENT_LIST_DIR}/../api/*.cc"
"${CMAKE_CURRENT_LIST_DIR}/../api/*.c"
)
file(GLOB FRAME_SOURCES
"${CMAKE_CURRENT_LIST_DIR}/../frame/*.cc"
"${CMAKE_CURRENT_LIST_DIR}/../frame/*.c"
)
file(GLOB FDFS_SOURCES
"${CMAKE_CURRENT_LIST_DIR}/../fdfs/*.cc"
"${CMAKE_CURRENT_LIST_DIR}/../fdfs/*.c"
)

list(FILTER BASE_SOURCES EXCLUDE REGEX ".*[Tt]est\\.cc$")
list(FILTER BASE_SOURCES EXCLUDE REGEX "CMakeCCompilerId\\.c$")
list(FILTER PROTO_SOURCES EXCLUDE REGEX ".*[Tt]est\\.cc$")
list(FILTER API_SOURCES EXCLUDE REGEX ".*[Tt]est\\.cc$")
list(FILTER FRAME_SOURCES EXCLUDE REGEX ".*[Tt]est\\.cc$")
list(FILTER FDFS_SOURCES EXCLUDE REGEX ".*[Tt]est\\.cc$")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/)
set(MY_TARGET fdfs_rpc_server)
list(FILTER FDFS_SOURCES EXCLUDE REGEX ".*/build/.*")

add_executable(${MY_TARGET}
    ${CMAKE_CURRENT_LIST_DIR}/${MY_TARGET}.cc
    ${BASE_SOURCES}
    ${API_SOURCES}
    ${PROTO_SOURCES}
    ${FRAME_SOURCES}
    ${FDFS_SOURCES}
)

# —— 包含路径 —— 
target_include_directories(${MY_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../proto
    ${CMAKE_CURRENT_LIST_DIR}/../base
    ${CMAKE_CURRENT_LIST_DIR}/../frame
    ${CMAKE_CURRENT_LIST_DIR}/../api
    ${CMAKE_CURRENT_LIST_DIR}/../proto

    # 第三方
    ${Protobuf_INCLUDE_DIRS}
    ${gRPC_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_HEADER}
    ${HIREDIS_HEADER}

    # FastDFS
    ${FASTDFS_INCLUDE_DIR}
)

# —— 链接库 —— 
target_link_libraries(${MY_TARGET} PRIVATE
    protobuf::libprotobuf
    gRPC::grpc++
    Threads::Threads

    ${HIREDIS_LIB}
    ${REDIS_PLUS_PLUS_LIB}

    ${MYSQL_CLIENT_LIB}

    # FastDFS
    ${FASTDFS_CLIENT_LIB}
    ${FASTCOMMON_LIB}
)
