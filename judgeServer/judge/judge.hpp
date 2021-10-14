#pragma once

#include <string>
#include <string_view>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <regex>

#include "check.hpp"
#include "lang_support.h"
#include "problem.hpp"
#include "log.hpp"

namespace  judge {

std::string readFile(std::string_view path);

#define __print_result(node,RESULT) std::cout << std::setw(12) << #node ": " << RESULT.node <<'\n';
#define print_result(RESULT)\
    __print_result(cpu_time,RESULT);\
    __print_result(real_time,RESULT);\
    __print_result(memory,RESULT);\
    __print_result(signal,RESULT);\
    __print_result(exit_code,RESULT);\
    __print_result(error,RESULT);\
    __print_result(result,RESULT);


namespace fs = std::filesystem;
const std::string judge_bin = "/usr/bin/judger_core";

enum {
    SUCCESS             = 0,
    INVALID_CONFIG      = -1,
    FORK_FAILED         = -2,
    PTHREAD_FAILED      = -3,
    WAIT_FAILED         = -4,
    ROOT_REQUIRED       = -5,
    LOAD_SECCOMP_FAILED = -6,
    SETRLIMIT_FAILED    = -7,
    DUP2_FAILED         = -8,
    SETUID_FAILED       = -9,
    EXECVE_FAILED       = -10,
    SPJ_ERROR           = -11,
    COMPILE_FAIL        = -12 // TODO
};

enum RESULT_MEAN {
    WRONG_ANSWER             = -1,
    CPU_TIME_LIMIT_EXCEEDED  = 1,
    REAL_TIME_LIMIT_EXCEEDED = 2,
    MEMORY_LIMIT_EXCEEDED    = 3,
    RUNTIME_ERROR            = 4,
    SYSTEM_ERROR             = 5
};

//评测的阶段
enum class STATUS : int {
    WAITING,
    ERROR,      //发生了错误
    JUDGING,
    END
};
std::string_view STATUS_to_string(STATUS s);

std::string_view result_to_string(RESULT_MEAN mean);

// 存结果 POD
struct result {
    int  cpu_time;
    int  real_time;
    long memory;
    int  signal;
    int  exit_code;
    int  error;
    int  result;
};

std::istream& operator>>(std::istream &__in,result &res);
std::ostream& operator<<(std::ostream &__on,result &res);

//更好的result
//struct result_detail {
    //int cpu_time;       //ms
    //int real_time;      //ms
    //float memory;       //mb
    //int signal;
    //int exit_code;
    //std::string error;
    //std::string result; //
    //result_detail& operator=(struct result& r){
        //cpu_time = r.cpu_time;
        //real_time = r.real_time;
        //memory = r.memory/1024.0/ 1024.0/1024.0; //todo
        //exit_code  = r.exit_code;
        //signal = r.signal;
        //error =  //error 与result 到底是什么意思
        //return *this;
    //}
//};



void exec(const char* cmd,std::ostream& __out);

//得到结果
result __judger(judge_args args);

//评测时发生的错误
class judge_error: public std::exception {
public:
    judge_error() =delete;
    explicit judge_error(std::string_view msg)
        : err_msg{msg}
    {}
	const char* what() const noexcept override { return err_msg.c_str(); }
	//STATUS stage() const noexcept { return _stage;}
private:
    //STATUS _stage;
    std::string err_msg;
};

struct Judger{
    Judger() = delete;
    Judger(Judger&&) = delete;
    /**
     * 构造函数
     *
     * @param code_full_path 要评测的代码的完整路径
     * @param lang 语言
     * @param pid  题目编号
     * @param problem_base 题目路径
     * @param code 代码
     *
     */
    explicit Judger
    (
        std::string_view code_full_path, //代码的路径
        SUPORT_LANG lang,      //语言
        std::string_view pid,       //problem id
        std::string_view problem_base,
        std::string_view code
    ):  work_path{ fs::path(code_full_path).parent_path() },
        code_name{ fs::path(code_full_path).filename() },
        lang{lang},
        pid{pid},
        problem_base{problem_base}
        {
            //1.创建对应的文件夹
            log_info("创建对应的文件夹",work_path);
            std::error_code ec;
            fs::create_directories(work_path,ec);
            if( ec.operator bool() ) //error_code 有值，也就是发生了错误
                throw judge_error(std::string("创建对应的文件夹时失败: ") + work_path.string());
                //throw std::runtime_error(std::string("创建对应的文件夹 失败") + work_path.string());
            //2.写入代码
            log_info("写入代码",code_full_path);
            std::ofstream __code(code_full_path.data());
            __code << code;
            __code.close();
        }

    
    auto run()-> std::tuple<STATUS,std::string,std::vector<result>>; //开始评测
    bool compile(judge_args & args); //编译
    const SUPORT_LANG lang; // 评测的语言
    const std::string pid;  // pid
    std::string_view problem_base; //题目的地址

    //哪里的位置，开始评测
    fs::path    work_path;
    std::string code_name;

};

}; // namespace judge
