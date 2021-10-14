#include <memory>
#include "lockfree_queue.hpp"
#include "threadpool.hpp"
#include "message.hpp"
#include "log.hpp"


using JUDGESEVER::judgeMessage;
using JUDGESEVER::resMessage;

template<size_t threadsize=4,size_t qsize=100000>
class judgeWork {
public:
    using resType = std::shared_ptr<resMessage>;
    void add(judgeMessage &m); //加入数据
    void judge(judgeMessage m); //memfunc
    bool get_result();
private:
    THREAD_POOL::threadpool thpool{threadsize};
    lockFreeQueue<resType,qsize> lfque;
};

template<size_t threadsize,size_t qsize>
void judgeWork<threadsize,qsize>::add(judgeMessage &jm){
    thpool.commit([this,jm](){
            log("=====开始评测=======");
            jm.debug();
            //1.前期检查
            //2.编译
            //3.
        });
};
