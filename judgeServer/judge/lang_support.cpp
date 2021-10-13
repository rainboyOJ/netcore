#include "lang_support.h"

std::string_view lang_to_string(SUPORT_LANG lang) {
    using namespace std::literals;
    switch(lang){
        case SUPORT_LANG::CPP:                   return "cpp"sv;
        case SUPORT_LANG::PY3:                   return "python3"sv;
        default:    return "UNSUPORT"sv;
    }
}

std::tuple<SUPORT_LANG,std::string> string_to_lang(std::string_view lang){
    if( lang == "cpp" || lang == "c++" ) 
        return std::make_tuple(SUPORT_LANG::CPP,".cpp");
    if( lang == "py3" || lang == "python3" ) 
        return std::make_tuple(SUPORT_LANG::PY3,".py");
    return std::make_tuple(SUPORT_LANG::UNSUPORT,"");
}

judge_args getCompileArgs(const SUPORT_LANG lang, const fs::path& cwd,std::string_view code_name){
    switch(lang){
        case SUPORT_LANG::PY3:      return compile_PY3_args(cwd,code_name);
        case SUPORT_LANG::CPP:      return compile_CPP_args(cwd,code_name);
        default:    throw ("UNSUPORT");
    }
}

judge_args getJudgeArgs(SUPORT_LANG lang,const fs::path& cwd,
        std::string_view code_name,
        std::string_view in_file_fullpath, //完整路径
        std::string_view out_file, //只要名字
        std::size_t __time, /* ms*/ std::size_t __memory /* mb */){
    switch(lang){
        case SUPORT_LANG::PY3:      return judge_PY3_args(cwd,code_name,in_file_fullpath,out_file,__time,__memory);
        case SUPORT_LANG::CPP:      return judge_CPP_args(cwd,code_name,in_file_fullpath,out_file,__time,__memory);
        default:    throw ("UNSUPORT");
    }
}


judge_args::judge_args(
        std::size_t max_cpu_time, std::size_t max_real_time, std::size_t max_process_number, std::size_t max_memory, std::size_t max_stack, std::size_t max_output_size,
        std::string seccomp_rule_name,
        const fs::path& cwd, const fs::path& input_path, const fs::path& output_path, const fs::path& error_path, const fs::path& log_path, const fs::path& exe_path,
        //std::vector<std::string> args,
        //std::vector<std::string> env,
        int gid, int uid)
: max_cpu_time{max_cpu_time},
    max_real_time{max_real_time},
    max_process_number{max_process_number},
    max_memory{max_memory},
    max_stack{max_stack},
    max_output_size{max_output_size},
    seccomp_rule_name{std::move(seccomp_rule_name)},
    cwd{cwd},
    exe_path{exe_path},
    input_path{input_path},
    output_path{output_path},
    error_path{error_path},
    log_path{log_path},
    gid{gid},
    uid{uid} {}

    judge_args::operator std::string() const { //转成string
        std::stringstream args_str;
        args_str << ' ';

        args_str <<"--max_cpu_time="<<max_cpu_time << ' ';
        args_str <<"--max_real_time="<<max_real_time<< ' ';
        args_str <<"--max_process_number="<<max_process_number<< ' ';
        args_str <<"--max_memory="<<max_memory<< ' ';
        args_str <<"--max_stack="<<max_stack<< ' ';
        args_str <<"--max_output_size="<<max_output_size<< ' ';
        args_str <<"--seccomp_rule_name="<<seccomp_rule_name<< ' ';

        args_str << "--cwd=" << cwd << ' ';
        args_str << "--input_path=" << input_path<< ' ';
        args_str << "--output_path=" << output_path<< ' ';
        args_str << "--error_path=" << error_path<< ' ';
        args_str << "--log_path=" << log_path<< ' ';
        args_str << "--exe_path=" << exe_path<< ' ';

        for (const auto& e : args) {
            args_str << "--args=" <<'"'  << e << '"' << ' ';
        }
        for (const auto& e : env) {
            args_str << "--env=" <<'"'  << e << '"' << ' ';
        }
        args_str << "--gid=" << gid << ' ';
        args_str << "--uid=" << uid << ' ';
        return args_str.str();
    }

compile_PY3_args::compile_PY3_args (const fs::path& cwd,std::string_view code_name)
    :judge_args(
            10*1000,90*1000,unlimit,unlimit,unlimit,128ull*(1ull<<30),
            "null", //不使用sec
            cwd,"/dev/null",
            cwd/"compile_output",
            cwd/"compile_error",
            cwd/"compile_log",
            "/usr/bin/python3",
            0,0)
{
    args = { "-m", "py_compile",cwd/code_name};
    env = {"PATH=/usr/bin"};
}

compile_CPP_args::compile_CPP_args (const fs::path& cwd,std::string_view code_name)
    :judge_args(
            10*1000,90*1000,unlimit,unlimit,unlimit,512ull*(1ull<<30),
            "null", //不使用sec
            cwd,"/dev/null",
            cwd/"compile_output",
            cwd/"compile_error",
            cwd/"compile_log",
            "/usr/bin/g++",
            0,0)
{
    args = { "-o",fs::path(code_name).stem(),std::string(code_name)};
    env = {"PATH=/usr/bin"};
}

judge_PY3_args::judge_PY3_args(const fs::path cwd,
        std::string_view code_name,
        std::string_view in_file,
        std::string_view out_file,
        std::size_t __time, /* ms*/ std::size_t __memory /* mb */)
    :judge_args(
            __time,10*__time,10,__memory*(1ull<<30),128_MB,128_MB,
            "null", //不使用sec
            cwd,
            in_file,    /*input_path*/
            cwd/out_file,
            cwd/"judge_error",
            cwd/"judge_log",
            "/usr/bin/python3",
            0,0) { 
        //log("judge_PY3_args",in_file);
        args = {cwd/code_name};
        env = {"PATH=/usr/bin"};
    }


judge_CPP_args::judge_CPP_args(const fs::path cwd,
        std::string_view code_name,
        std::string_view in_file,
        std::string_view out_file,
        std::size_t __time, /* ms*/ std::size_t __memory /* mb */)
    :judge_args(
            __time,10*__time,10,__memory*(1ull<<30),128_MB,128_MB,
            "null", //不使用sec
            cwd,
            in_file,    /*input_path*/
            cwd/out_file,
            cwd/"judge_error",
            cwd/"judge_log",
            fs::path(code_name).stem(),
            0,0) { 
        //args = {};
        env = {"PATH=/usr/bin"};
    }
