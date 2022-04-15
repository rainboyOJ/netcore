#include "websocket_manager.h"


namespace rojcpp {

bool WS_manager::regist(std::string id, int fd){
    std::lock_guard<std::mutex> lock(m_mutex);
    //检查是否存在一样的id
    if( u_map.find(id) != u_map.end() )
        return false;
    //检查fd 是否已经被注册
    if( fd_set.find(fd) != fd_set.end() )
        return false;

    //注册
    u_map.emplace(id,fd);
    fd_set.insert(fd);

    return true;
}

bool WS_manager::unregist(std::string id,int fd){
    std::lock_guard<std::mutex> lock(m_mutex);

    u_map.erase(id);
    fd_set.erase(fd);
}

//@desc 通过注册的id 来发送信息
void WS_manager::send_msg_by_id(std::string & id,std::string && msg,bool close){
    auto id_iter = u_map.find(id);
    if( id_iter != u_map.end() ){
        send_ws_string( id_iter->second,msg,close);
    }
    else {
        LOG_DEBUG("not find fd from id : %s",id.c_str());
    }
}

} // namespace rojcpp end
