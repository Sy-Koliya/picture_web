cmake_minimum_required(VERSION 3.23)
project(async_server )

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


# —— 源码文件收集 —— 
file(GLOB_RECURSE BASE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../base/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../base/*.c"
)
file(GLOB_RECURSE PROTO_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../proto/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../proto/*.c"
)
file(GLOB_RECURSE API_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../api/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../api/*.c"
)
file(GLOB_RECURSE FRAME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../frame/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../frame/*.c"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/)
set(MY_TARGET HttpServer_test)

add_executable(${MY_TARGET}
    ${CMAKE_CURRENT_LIST_DIR}/${MY_TARGET}.cc
    ${BASE_SOURCES}
    ${API_SOURCES}
    ${PROTO_SOURCES}
    ${FRAME_SOURCES}
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
