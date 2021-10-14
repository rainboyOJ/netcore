#include "judge.hpp"

namespace  judge {

std::string readFile(const char * path){
    std::stringstream ss;
    std::ifstream f(path);
    ss << f.rdbuf();
    return ss.str();
}

std::istream& operator>>(std::istream &__in,result &res){
    __in >> res.cpu_time 
        >> res.real_time 
        >> res.memory    
        >> res.signal    
        >> res.exit_code 
        >> res.error     
        >> res.result;
    return __in;
}
std::ostream& operator<<(std::ostream &__on,result &res){
    __on << res.cpu_time << ' '
        << res.real_time << ' '
        << res.memory    << ' '
        << res.signal    << ' '
        << res.exit_code << ' '
        << res.error     << ' '
        << res.result << '\n';
    return __on;
}

void exec(const char* cmd,std::ostream& __out) {
    char buffer[128];
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error(std::string("popen() failed!") + __FILE__ + " line: " +  std::to_string(__LINE__).c_str());
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            __out << buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    //return result;
}


std::string_view result_to_string(RESULT_MEAN mean) {
    using namespace std::literals;
    switch(mean){
        case WRONG_ANSWER:              return "WRONG_ANSWER"sv;
        case CPU_TIME_LIMIT_EXCEEDED:   return "CPU_TIME_LIMIT_EXCEEDED"sv;
        case REAL_TIME_LIMIT_EXCEEDED:  return "REAL_TIME_LIMIT_EXCEEDED"sv;
        case MEMORY_LIMIT_EXCEEDED:     return "MEMORY_LIMIT_EXCEEDED"sv;
        case RUNTIME_ERROR:             return "RUNTIME_ERROR"sv;
        case SYSTEM_ERROR:              return "SYSTEM_ERROR"sv;
        default:    return "UNKOWN"sv;
    }
}


result __judger(judge_args args){
    std::stringstream ss;
    //log("参数",(judge_bin + static_cast<std::string>(args)).c_str() );
    //std::cout << std::endl ;
    exec( ( judge_bin + static_cast<std::string>(args)).c_str() ,ss);
    result RESULT;
    ss >> RESULT.cpu_time;
    ss >> RESULT.real_time;
    ss >> RESULT.memory;
    ss >> RESULT.signal;
    ss >> RESULT.exit_code;
    ss >> RESULT.error;
    ss >> RESULT.result;
    return RESULT;
}


bool Judger::compile(judge_args & args){
    //compile_python_args(work_path, code_file_name.c_str());
    auto res = __judger(args);
    print_result(res);
    return res.exit_code == 0 && res.error == 0;
}


std::string_view STATUS_to_string(STATUS s){
    using namespace std::literals;
    switch(s){
        case STATUS::WAITING:       return "WAITING"sv;
        case STATUS::JUDGING:       return "JUDGING"sv;
        case STATUS::ERROR:       return "ERROR"sv;
        //case STATUS::COMPILE_ERROR: return "COMPILE_ERROR"sv;
        case STATUS::END:       return "END"sv;
    }
}

//TODO return std::vector<result>
//返回值 msg类型
auto Judger::run()->
        std::tuple<STATUS,std::string,std::vector<result>> 
{
    //编译
    try {

        auto args = getCompileArgs(lang, work_path, code_name);
        if( !compile(args) ){
            std::string msg = readFile(args.error_path.c_str());
            if( msg.length() == 0)
                msg =  readFile(args.log_path.c_str());
            //auto a = std::make_tuple(1,2,3);
            //return std::make_tuple(STATUS::COMPILE_ERROR,msg, std::vector<result> {});
            throw  judge::judge_error(msg);
        }
    }
    catch(...){
        throw std::runtime_error("compile throw error");
    }

    try {
        Problem p(problem_base,pid); //根据pid拿到 数据列表
        std::vector<result> results{};
        auto time_limit = 1000;
        auto memory_limit = 128;

        for(int i=0;i<p.input_data.size();++i){    // 循环进行判断
            auto& in_file = p.input_data[i].second;
            auto& out_file = p.output_data[i].second;
            //log("in_file",in_file);
            //log("out_file",out_file);
            std::string user_out_file = "out"+std::to_string(i);
            auto args = getJudgeArgs(lang, work_path, code_name, in_file, user_out_file, time_limit, 128+memory_limit);
            //log(i, static_cast<std::string>(args) );
            auto res = __judger(args);
            if( res.error != 0  || res.result !=0)
                results.push_back(res);
            else  {
                //查检memory
                if( res.memory >= 1024ull * 1024 *1024 * memory_limit){
                    res.result = MEMORY_LIMIT_EXCEEDED;
                }
                else {
                    //res.result = MEMORY_LIMIT_EXCEEDED;
                    //答案检查
                        //log_one(args.output_path);
                        //log_one(out_file);
                    if( !cyaron::Check::noipstyle_check(args.output_path.c_str(), out_file) ){
                        res.result = WRONG_ANSWER;
                    }
                }
                results.push_back(res);
            }
        }
        return std::make_tuple(STATUS::END,"", std::move(results));
    }
    catch(...){
    }
}

}; // namespace judge
