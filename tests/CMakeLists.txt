# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)

# 项目名称
project(netcore_test)

# 指定编译flag
set(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_CXX_FLAGS "-std=c++20")

# 设置宏
add_definitions(-DDEBUG)

# 指定include 的头文件的位置
include_directories(../include/netcore)

# 添加代码
aux_source_directory(../src DIR_SRCS)



# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量

add_executable(sever multi_server.cpp ${DIR_SRCS})

