#include "websocket_manager.h"


namespace netcore {

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

void WS_manager::unregist(std::string id,int fd){
    std::lock_guard<std::mutex> lock(m_mutex);

    u_map.erase(id);
    fd_set.erase(fd);
}

int WS_manager::get_fd(const std::string & key){
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = u_map.find(key);
    if( it == u_map.end() )
        return -1; //没有找到
    return it->second;
}

void WS_manager::close_by_key(const std::string &key){
    auto _fd = get_fd(key);
    if( _fd != -1)
        this->Close(_fd);
}

//@desc 通过注册的id 来发送信息
void WS_manager::send_msg_by_id(std::string & id,std::string && msg,bool close){
    //std::cout << "=> send_msg_by_id , id: " << id << std::endl;
    auto _fd = get_fd(id);
    //std::cout << "=> send_msg_by_id id_iter.first :  " << id_iter->first << std::endl;
    //std::cout << "=> send_msg_by_id id_iter.second:  " << id_iter->second << std::endl;
    if( _fd != -1 ){
        send_ws_string( _fd,msg,close);
    }
    else {
        LOG(DEBUG) << "not find fd from id : " << id;
    }
}

} // namespace netcore end
