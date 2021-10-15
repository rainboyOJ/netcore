#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

#include "base64.hpp"
#include "judge/judge.hpp"
#include "log.h"

//信息的传输格式
namespace JUDGESEVER {

//发送的信息
class judgeMessage : public std::enable_shared_from_this<judgeMessage> {
public:
    using SHR_PTR = std::shared_ptr<judgeMessage>;
    judgeMessage() = default;
    judgeMessage(int time,int memory,std::string_view pid ,std::string_view uid,std::string_view lang,std::string_view code)
        :time{time},memory{memory},pid{pid},uid{uid},lang{lang},code{code}
    {}

    judgeMessage(int time,int memory,int pid ,int uid,std::string_view lang,std::string_view code)
        :time{time},memory{memory},pid{std::to_string(pid)},uid{std::to_string(uid)},lang{lang},code{code}
    {}

    int         time{1000};   //时间限制 ms
    int         memory{128}; //内存限制 mb
    std::string   pid;    //要评测的题目的id
    std::string   uid;    //标识
    int         socket;
    std::string lang;
    std::string code;
private:
    enum class MSGD_STATUS{ start, new_line, space, key, value };
    MSGD_STATUS status{MSGD_STATUS::start};
    std::string key;
    std::string value;
public:
    void debug() const{
        LOG_DEBUG("time= %d",time);
        LOG_DEBUG("memory= %d",memory);
        LOG_DEBUG("uid= %s",uid.c_str());
        LOG_DEBUG("pid= %s",pid.c_str());
        LOG_DEBUG("lang= %s",lang.c_str());
        LOG_DEBUG("code= %s",code.c_str());
    }
    std::string encode(){
        std::stringstream ss;
        ss << "lang " << lang << '\n';
        ss << "time " << time << '\n';
        ss << "memory " << memory << '\n';
        ss << "pid " << pid << '\n';
        ss << "uid " << uid << '\n';
        ss << "socket " << socket << '\n';
        ss << "code " << Base64::Encode(code.c_str(), code.size()) << '\n';
        ss << '\n';
        return ss.str();
    }
    bool decode(unsigned char c){
        switch( status ){
            case MSGD_STATUS::start:
                key.clear();
                key.push_back(c);
                status = MSGD_STATUS::key;
                return false;
            case MSGD_STATUS::space:
                value.clear();
                value.push_back(c);
                status = MSGD_STATUS::value;
                return false;
            case MSGD_STATUS::key:
                if( c == ' '){
                    status = MSGD_STATUS::space;
                }
                else key.push_back(c);
                return false;
            case MSGD_STATUS::new_line:
                if(c == '\n') { return true; }
                key.clear();
                key.push_back(c);
                status = MSGD_STATUS::key;
                return false;
            case MSGD_STATUS::value:
                if( c == '\n'){
                    if( key == "lang")      lang = std::move(value);
                    else if( key == "code") code = Base64::Decode(value.c_str(), value.size());
                    else if( key == "time") time = atoi(value.c_str());
                    else if( key == "uid")  uid = std::move(value);
                    else if( key == "pid")  pid = std::move(value);
                    else if( key == "memory") memory = atoi(value.c_str());
                    else if( key == "socket") socket = atoi(value.c_str());
                    status = MSGD_STATUS::new_line;
                }
                else value.push_back(c);
                return false;
        }
        return false;
    }
};


class resMessage : public std::enable_shared_from_this<resMessage>{
public:
    using SHR_PTR = std::shared_ptr<resMessage>;

    int                        socket; //哪个socket需要写
    judge::STATUS              status; // 评测的状态
    std::string                msg;     //相关的信息
    std::vector<judge::result> results; //结果集

    resMessage() = default;
    resMessage(int socket,judge::STATUS status,std::string_view msg,std::vector<judge::result>&& results)
        :socket{socket},status{status},msg{msg},results{results}
    { }

    void debug() const{
        LOG_DEBUG("socket=%d ",socket);
        LOG_DEBUG("status= %d",static_cast<int>(status));
        LOG_DEBUG("status_mean = %s",judge::STATUS_to_string(status).data());
        LOG_DEBUG("msg = %s",msg.c_str());
        LOG_DEBUG("results size = %d",results.size());
        for (const auto& e : results) {
            LOG_DEBUG( "%d %d %d %d %d %d %d", e.result,e.cpu_time,e.real_time,e.memory,e.error,e.exit_code,e.signal);
        }
    }

    bool decode(unsigned char c){
        if( c == '\n' && pre_c == '\n') {
            ss >> socket;
            int sta;
            ss >> sta;
            status = static_cast<judge::STATUS>(sta);
            ss >> msg;
            msg = Base64::Decode(msg.c_str(), msg.length());
            judge::result t;
            while( ss >> t.cpu_time 
                    >> t.real_time 
                    >> t.memory    
                    >> t.signal    
                    >> t.exit_code 
                    >> t.error     
                    >> t.result) {
                results.push_back(t);
            }

            return true;
        }
        pre_c = c;
        ss << c;
        return false;
    }
    std::string encode(){
        std::stringstream ss;
        ss << socket << '\n';
        ss << static_cast<int>(status) << '\n';
        ss << Base64::Encode(msg.c_str(), msg.length()) << '\n';
        for (auto& e : results) {
            //ss << e;
            ss << e.cpu_time << ' '
                << e.real_time << ' '
                << e.memory    << ' '
                << e.signal    << ' '
                << e.exit_code << ' '
                << e.error     << ' '
                << e.result << '\n';
        }
        ss << '\n';
        return ss.str();
    }

private:
    unsigned char pre_c{0};
    std::stringstream ss;
};



} //namespace JudgeSever
