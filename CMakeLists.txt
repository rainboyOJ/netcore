# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)

# 项目名称
project(netcore)

# 指定编译flag
set(CMAKE_CXX_FLAGS "-std=c++20")
add_definitions(-D__NETCORE__DEBUG__)
#if(${CMAKE_BUILD_TYPE} EQUAL "debug")
if (CMAKE_BUILD_TYPE STREQUAL "debug")
    message("debug 2")
    #add_definitions("-g")
    #SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
    #SET(CMAKE_CXX_FLAGS_DEBUG "-g")
    # 设置宏
    #add_definitions(-DDEBUG)
endif (CMAKE_BUILD_TYPE STREQUAL "debug")


if(EXISTS ${USER_CONFIG_PATH})
    message(STATUS "使用用户的配置文件 ${USER_CONFIG_PATH}")
    add_definitions(-DUSER_CONFIG)
    add_compile_options(-include ${USER_CONFIG_PATH})
else()
    message(STATUS "使用默认配置文件 ${PROJECT_SOURCE_DIR}/__default_config.hpp")
endif(EXISTS ${USER_CONFIG_PATH})

# 指定include 的头文件的位置
include_directories(include/netcore)
include_directories(lib)
include_directories(lib/font)

## 添加代码
aux_source_directory(lib LIB_SRCS)
aux_source_directory(src CORE_SRCS)
set(SOURCES 
    ${DIR_SRCS} 
    ${CORE_SRCS}
    lib/easyloggingpp/easylogging++.cc
    )
#aux_source_directory(core/hs_utils HTTPSERVER_UTILS_SRC)

# 设置链接库路径

# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量

#add_executable(test ${DIR_SRCS})
#add_library(netcore STATIC ${DIR_SRCS} ${CORE_SRC} ${HTTPSERVER_UTILS_SRC} lib/easyloggingpp/easylogging++.cc)

#if( NOT DEFINED NOT_ADD_NETCORE_TESTS

## demo
#include(./demo/CMakeLists.txt)

#add_definitions(-DDEBUG)
add_executable(server demo/multi_server.cpp ${SOURCES})
target_link_libraries(server pthread)
