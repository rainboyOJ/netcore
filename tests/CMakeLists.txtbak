# 指定编译flag

# 设置宏

# 指定include 的头文件的位置
#include_directories(../core)
#include_directories(../lib)


add_definitions(-DDEBUG)

file(GLOB TESTS "*.cpp")
foreach(f ${TESTS})
    string(REPLACE ".cpp" "" testname ${f})
    string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/ "" name ${testname})
    add_executable(${name} ${f})
    target_link_libraries(${name} netcore pthread )
    #target_compile_options(${name} PUBLIC ${MYSQL_CFLAGS})
endforeach(f)
