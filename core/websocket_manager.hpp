//websocket 管理
// @desc 实现websocket连接的管理,注册后可以通过多线程的发送信息
//      
//#include <tmux>
#include <string>
#include <string_view>
#include <mutex>
#include <unordered_map>

namespace rojcpp {

class WS_manager {
public:
    WS_manager& get_instance(){
        static WS_manager ws_manager;
        return ws_manager;
    }

    //注册
    bool regist(std::string_view id,int fd);
    //反注册
    bool unregist(std::string_view id);
    //是否存活
    bool is_alive(std::string_view id);

    //发送信息
    void send_message(std::string_view id,std::string msg){
        std::lock_guard<std::mutex> lock(m_mutex);
    }

private:
    WS_manager() = default;
    std::mutex m_mutex;
    std::unordered_map<std::string,int> m_unmap;
    std::array<std::mutex, 4> m_mutex_arr; //TODO
};

} // namespace rojcpp end
