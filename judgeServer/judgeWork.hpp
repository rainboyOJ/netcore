#include <memory>
#include "lockfree_queue.hpp"
#include "threadpool.hpp"
#include "message.hpp"
#include "log.hpp"
#include "judge/judge.hpp"


using JUDGESEVER::judgeMessage;
using JUDGESEVER::resMessage;

template<size_t threadsize=4,size_t qsize=100000>
class judgeWork {
public:
    judgeWork() = delete;
    explicit judgeWork(std::string_view problem_base,std::string_view judge_base_path)
        : problem_base{problem_base},judge_base_path{judge_base_path}
    {}
    using resType = std::shared_ptr<resMessage>;
    void add(judgeMessage &m); //加入数据
    void judge(judgeMessage m); //memfunc
    bool get_result();

    std::filesystem::path make_code_path(std::string_view uid,std::string ext) const{
        return judge_base_path / uid 
            / (std::string("main") + ext);
    }

    std::filesystem::path make_code_path(int uid,std::string ext) const{
        return make_code_path(std::to_string(uid), ext);
    }
    auto get_problem_base() const {
        return problem_base;
    }

private:

    THREAD_POOL::threadpool thpool{threadsize};
    lockFreeQueue<resType,qsize> lfque;
    fs::path problem_base; //题目的路径
    fs::path judge_base_path; //评测的路径
};

template<size_t threadsize,size_t qsize>
void judgeWork<threadsize,qsize>::add(judgeMessage &jm){
    thpool.commit([this,jm](){
            log("=====开始评测=======");
            //jm.debug();
            try {
                //1. 是否是支持的语言
                auto [lang,ext]  = string_to_lang(jm.lang);
                if( lang == SUPORT_LANG::UNSUPORT)
                    throw judge::judge_error("不支持的语言 " + jm.lang);

                //2.创建Judger对象 进行相应的
                judge::Judger jd( 
                        this->make_code_path(jm.uid, ext).c_str(),
                        lang, jm.pid, this->get_problem_base().c_str(),
                        jm.code
                        );
                //if( jd.compile(judge_args &args))
            }
            catch( judge::judge_error & e){
                log_error(e.what());
            }
            //1.前期检查
            //2.编译
            //3.
        });
};
