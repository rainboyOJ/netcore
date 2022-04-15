/**
 * 参考 https://github.com/active911/fastcache 的Cache容器,
 * 
 * target:
 *  1. 取代使用Redis,Redis还是比较适合较大型的项目
 *  2. 使用c++17
 *  
 * Features
 *
 *  1. 支持expire
 *  2. 使用分片,加快速度
 */

#pragma once
#include <thread>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <cxxabi.h>

namespace rojcpp {


#define GET_TYPE_NAME(type) abi::__cxa_demangle(typeid(type).name(),0,0,0)

//工具类
template<typename T1,typename T2>
struct has_append_member_function {
    template<typename U1,typename U2>
    static auto check(int) -> decltype(std::declval<U1>().append( std::declval<U2>() ),std::true_type());

    //sink hole function
    template<typename U1,typename U2>
    static std::false_type check(...);

    static constexpr bool value = decltype(check<T1,T2>(0))::value;
};

   
template<typename Key_t,typename Val_t ,std::size_t Shard_size = 255u>
class Fastcache {

    //@desc 转成秒
    template<typename U>
    static inline std::size_t asSeconds(U _duration_) {
        return std::chrono::duration_cast<std::chrono::seconds>(_duration_).count();
    }

    static inline std::size_t getNowSeconds() {
        return asSeconds(std::chrono::system_clock::now().time_since_epoch());
    }


    struct CacheItem {
        Val_t       data;             //数据
        std::size_t expiration{0}; //结束的时间 seconds

        CacheItem() = default;

        template<typename U>
        CacheItem(U && _d, std::size_t expired_duration_time){
            data = std::forward<U>(_d);
            expiration = getNowSeconds() + expired_duration_time;
        }

        //追加
        //pre_split,是否在前面 添加一个 字符,固定为,
        void append(const Val_t & _data,std::size_t new_exp,bool use_pre_split_char = true) {
            static_assert( has_append_member_function<Val_t,const Val_t &>::value ,"CacheItem append fail");

            if( use_pre_split_char )
                data.append(",");
            data.append(_data);
            expiration = new_exp;
        }

        //转成字符串
        std::string dumps(){
            std::ostringstream oss;
            oss << expiration << " ";
            if constexpr ( std::is_same_v<Val_t, std::string>)
                oss << data;
            else
                throw std::string("unsuport dumps type ") + GET_TYPE_NAME(Val_t);
            return oss.str();
        }

        //TODO 从字符串读取
        void loads(const std::string & _str);

        bool expired(std::size_t now_time = getNowSeconds() ){
            return expiration == 0 
                    ? false  // 没有设置结束的时间
                    : now_time > expiration;
        }
    };


    //分片,一片数据
    struct Shard {
        std::mutex mtx;
        std::unordered_map<Key_t, CacheItem> _container;

        //@desc 删除过期的key
        void del_expired_keys(std::size_t now_time = getNowSeconds() ) {
            std::lock_guard<std::mutex> _lock(mtx);
            for( auto it = _container.begin() ; it != _container.end();) {
                if( it->second.expired(now_time) )
                    it = _container.erase(it);
                else
                    ++it;
            }
        }
    };

public:
    //ctor
    Fastcache() : m_shards{Shard_size}
    {
        //del_expired_thread
        expired_check_thread = std::thread(&Fastcache::expired_check_func,this);
    }

    ~Fastcache(){
        run_flag.store(false);
        if( expired_check_thread.joinable())
            expired_check_thread.join();
    }

    //@desc 得到所有key的数量
    std::size_t total_size();

    //@desc 设定一个值
    void set(Key_t&& key,Val_t&& val,std::size_t expiration = 10);

    //@desc key是否存在
    bool exists(Key_t&& key);

    //@desc 删除一个key
    std::size_t del(Key_t&& key);

    //@desc 得到一个key的值
    std::tuple<bool, Val_t > get(Key_t&& key); 

    //@desc 追加
    template<typename K,typename V>
    void append(K&& key,V&& _data,std::size_t expiration = 10);

protected:
    //desc 检查每一个shard里过期的key
    void expired_check_func();

    //@desc 计算key对应的分片
    inline std::size_t calc_index(Key_t&& key);
private:
    std::hash<Key_t> m_hash_tool;
    std::vector<Shard> m_shards;
    std::thread expired_check_thread;
    std::atomic_bool run_flag{true}; // 运行标记
};

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
std::size_t 
    Fastcache<Key_t,Val_t,Shard_size>::
    calc_index(Key_t&& key)
{
    return m_hash_tool(key) % Shard_size;
}

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
std::size_t 
    Fastcache<Key_t,Val_t,Shard_size>::
    total_size()
{
    std::size_t tot{0};
    for (auto& e : m_shards) {
        std::lock_guard<std::mutex> lck(e.mtx);
        tot += e._container.size();
    }
    return tot;
}


template<typename Key_t,typename Val_t ,std::size_t Shard_size>
void Fastcache<Key_t,Val_t,Shard_size>::
    set(Key_t&& key,Val_t&& val,std::size_t expiration)
{
    std::size_t index = calc_index(std::forward<Key_t>(key));
    auto & _shard = m_shards[index];
    std::lock_guard<std::mutex> lck(_shard.mtx);
    _shard._container[key] 
        = CacheItem(std::forward<Val_t>(val),expiration);
}

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
bool
    Fastcache<Key_t,Val_t,Shard_size>::
    exists(Key_t&& key)
{
    std::size_t index = calc_index(std::forward<Key_t>(key));
    auto & _shard = m_shards[index];
    std::lock_guard<std::mutex> lck(_shard.mtx);
    return _shard._container.find(key) != _shard._container.end();
}

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
std::size_t
    Fastcache<Key_t,Val_t,Shard_size>::
    del(Key_t&& key)
{
    std::size_t index = calc_index(std::forward<Key_t>(key));
    auto & _shard = m_shards[index];
    std::lock_guard<std::mutex> lck(_shard.mtx);
    return _shard._container.erase(key);
}

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
std::tuple<bool, Val_t > 
    Fastcache<Key_t,Val_t,Shard_size>::
    get(Key_t&& key)
{
    std::size_t index = calc_index(std::forward<Key_t>(key));
    auto & _shard = m_shards[index];
    std::lock_guard<std::mutex> lck(_shard.mtx);
    try {
        auto val = _shard._container.at(key);
        //std::cout << val.data << " : expired at : " << val.expiration  << std::endl;
        //std::cout << "now :" << getNowSeconds() << std::endl;
        if( val.expired() )
            _shard._container.erase(key);
        return  std::make_tuple(true, std::move(val.data));
    }
    catch(std::exception & e){
        return  std::make_tuple(false,Val_t{});
    }
}

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
void
    Fastcache<Key_t,Val_t,Shard_size>::
    expired_check_func()
{
    using namespace std::chrono_literals;
    while( run_flag.load() == true) {
        std::this_thread::sleep_for(1s);

        auto now_time = getNowSeconds();
        for (auto& e : m_shards) {
            e.del_expired_keys(now_time);
        }
    }
}

template<typename Key_t,typename Val_t ,std::size_t Shard_size>
template<typename K,typename V>
void
    Fastcache<Key_t,Val_t,Shard_size>::
    append(K&& key,V&& val,std::size_t expiration)
{
    std::size_t index = calc_index(std::forward<Key_t>(key));
    auto & _shard = m_shards[index];
    std::lock_guard<std::mutex> lck(_shard.mtx);

    auto iter = _shard._container.find(key);
    // 没有找到,创建一个
    if ( iter == _shard._container.end() ) 
    {
        _shard._container[key] 
            = CacheItem(std::forward<Val_t>(val),expiration);
    }
    else { //找到了,追加
        iter->second.append(val,expiration);
    }
}

} // end namespace rojcpp

