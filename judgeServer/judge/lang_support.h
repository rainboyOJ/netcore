//语言支持 根据不同的语言生成来的 编译 与 评测参数
#pragma once
#include <string_view>
#include <tuple>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

const int unlimit = 0;

using ull = unsigned long long;
constexpr ull operator ""_MB(ull a){
    return a*1024*1024*1024;
}

constexpr std::size_t operator ""_SEC(ull a){
    return a*1000;
}

//支持的语言
enum class SUPORT_LANG {
    CPP,
    PY3,
    UNSUPORT
};

//  judge compile args factory by lang
enum class JUDGE_ARGS_TYPE {
    COMPILE,
    JUDGE,
};

constexpr auto COMPILE = JUDGE_ARGS_TYPE::COMPILE;
constexpr auto JUDGE   = JUDGE_ARGS_TYPE::COMPILE;

std::string_view lang_to_string(SUPORT_LANG lang);

auto string_to_lang(std::string_view lang)
    ->std::tuple<SUPORT_LANG,std::string>;

//基类
struct judge_args {
    explicit judge_args(
            std::size_t max_cpu_time, std::size_t max_real_time, std::size_t max_process_number, std::size_t max_memory, std::size_t max_stack, std::size_t max_output_size,
            std::string seccomp_rule_name,
            const fs::path& cwd, const fs::path& input_path, const fs::path& output_path, const fs::path& error_path, const fs::path& log_path, const fs::path& exe_path,
            int gid, int uid);

    std::size_t max_cpu_time;
    std::size_t max_real_time;
    std::size_t max_process_number;
    std::size_t max_memory;//  512mb
    std::size_t max_stack;
    std::size_t max_output_size;
    std::string seccomp_rule_name;
    fs::path cwd;
    fs::path input_path;
    fs::path output_path;
    fs::path error_path;
    fs::path log_path;
    fs::path exe_path;

    std::vector<std::string> args;
    std::vector<std::string> env;
    int gid;
    int uid;

    operator std::string() const; 
};


//参数 1.编译的目录 2.评测的代码名字
#define create_compile_args(name) struct compile_##name##_args : public judge_args {\
    compile_##name##_args() = delete;\
    explicit compile_##name##_args(const fs::path& cwd,std::string_view code_name);\
};

#define create_judge_args(name) struct judge_##name##_args: public judge_args {\
    judge_##name##_args() = delete;\
    explicit judge_##name##_args(const fs::path cwd,\
            std::string_view code_name,\
            std::string_view in_file_fullpath,\
            std::string_view out_file,\
            std::size_t __time, /* ms*/ std::size_t __memory /* mb */);\
};

create_compile_args(PY3)
create_compile_args(CPP)

create_judge_args(PY3)
create_judge_args(CPP)


//得到语言 对应的编译参数
judge_args getCompileArgs(const SUPORT_LANG lang, const fs::path& cwd,std::string_view code_name);

//得到语言 对应的评测参数
judge_args getJudgeArgs(SUPORT_LANG lang,const fs::path& cwd,
            std::string_view code_name,
            std::string_view in_file_fullpath, //完整路径
            std::string_view out_file, //只要名字
            std::size_t __time, /* ms*/ std::size_t __memory /* mb */);
