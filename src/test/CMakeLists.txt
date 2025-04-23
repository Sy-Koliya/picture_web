cmake_minimum_required(VERSION 3.10)
project(TestThrPool C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ROOT_DIR 指向项目根（test 目录的上一级）
get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ROOT_DIR}/test")
# 把 base/ 和 frame/ 加入搜索路径
include_directories(
    ${ROOT_DIR}/base
    ${ROOT_DIR}/frame
)

# 收集源文件
file(GLOB_RECURSE BASE_SOURCES
    ${ROOT_DIR}/base/*.c
    ${ROOT_DIR}/base/*.cc
)
file(GLOB_RECURSE FRAME_SOURCES
    ${ROOT_DIR}/frame/*.cpp
)

set(my_target_test socketpool_test)
add_executable(${my_target_test}
    ${BASE_SOURCES}
    ${FRAME_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/${my_target_test}.cc
)

# 链接 pthread
target_link_libraries(${my_target_test} PRIVATE pthread)
target_link_libraries(${my_target_test} PRIVATE dl)