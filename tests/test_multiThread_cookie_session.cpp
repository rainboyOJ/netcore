//测试的session_manager 在多线程下会不会出错
#include <iostream>
#include <memory>
#include <random>
#include "threadpool.hpp"

#include "session_manager.hpp"
#include "response.hpp"

using namespace std;

using namespace rojcpp;

struct Random {
    random_device rd;
    mt19937 engine{rd()};
    uniform_int_distribution<long long> dis; // in [0,0x7fffffffffffffff]
    Random(){}
    Random(int l,int r){ dis = uniform_int_distribution<long long> (l,r); }

    int operator()(){ return dis(engine); }
    template<typename U> //产生一定范围内的随机数
    U operator()(U l,U r){ return dis(engine) % ( r-l+1 ) + l; }

    Random create(int l,int r){ return Random(l,r); } //工厂模式
} rnd;

//动态的创建,
//动态的删除
//动态的获得值
int main(){
    //线程池
    //session_manager::get().create_session(const std::string &name, std::size_t expire)
    auto thpool = std::make_shared<THREAD_POOL::threadpool>();
    using namespace std::literals;

    for(int i=1;i<=100;++i){
        auto __rnd = rnd(1,3);
        if( __rnd == 1)
            thpool->commit([]{
                    std::cout << "创建 session" << std::endl;
                    auto s = session_manager::get().create_session("CSESSIONID", 10);
                    std::cout << s->get_id() << std::endl;
                });
        //else if ( __rnd == 2)
            //thpool->commit([]{
                    //std::cout << "删除 session" << std::endl;
                    //session_manager::get().del_session("CSESSIONID", 10);
                //});
    }
    for(int i=1;i<=100;++i){
        std::cout <<  "check "<< i << std::endl;
        session_manager::get().check_expire();
        std::this_thread::sleep_for(1s);
        std::cout << "size " << session_manager::get().get_session_size() << std::endl;
    }
    return 0;
}
