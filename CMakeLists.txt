# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)

# 项目名称
project(netcore)

# 指定编译flag
set(CMAKE_CXX_FLAGS "-std=c++17")

#add_definitions("-g")

# 设置宏
add_definitions(-D DEBUG)

# 指定include 的头文件的位置
include_directories(core)
include_directories(lib)
include_directories(lib/font)

# 添加代码
aux_source_directory(lib DIR_SRCS)
aux_source_directory(core CORE_SRC)

# 设置链接库路径

# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量

#add_executable(test ${DIR_SRCS})
add_library(netcore STATIC ${DIR_SRCS} ${CORE_SRC})


add_subdirectory(tests)
