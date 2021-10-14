#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

#include "base64.hpp"
#include "judge/judge.hpp"
#include "log.hpp"

//信息的传输格式
namespace JUDGESEVER {

//message decode status
#define msg_dbg_out(one) std::cout << #one << ": " <<  one  << std::endl

//发送的信息
class judgeMessage {
public:
    judgeMessage() = default;
    judgeMessage(int time,int memory,int pid ,int uid,std::string_view lang,std::string_view code)
        :time{time},memory{memory},pid{pid},uid{uid},lang{lang},code{code}
    {}

    int         time{1000};   //时间限制 ms
    int         memory{128}; //内存限制 mb
    int         pid{0};    //标识
    int         uid{0};    //标识
    int         socket;
    std::string lang;
    std::string code;
private:
    enum class MSGD_STATUS{ start, new_line, space, key, value };
    MSGD_STATUS status{MSGD_STATUS::start};
    std::string key;
    std::string value;
public:
    void debug(){
        msg_dbg_out(time);
        msg_dbg_out(memory);
        msg_dbg_out(uid);
        msg_dbg_out(lang);
        msg_dbg_out(code);
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
                    else if( key == "uid")  uid = atoi(value.c_str());
                    else if( key == "pid")  pid = atoi(value.c_str());
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


class resMessage {
public:
    int                        socket; //哪个socket需要写
    judge::STATUS              status; // 评测的状态
    std::string                msg;     //相关的信息
    std::vector<judge::result> results; //结果集

    void debug(){
        log_one(socket);
        log("status",static_cast<int>(status));
        log_one(msg);
        log_one(results.size());
        for (const auto& e : results) {
            log(e.result,e.cpu_time,e.real_time,e.memory,e.error,e.exit_code,e.signal);
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
            while(ss >> t) {
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
            ss << e;
        }
        ss << '\n';
        return ss.str();
    }

private:
    unsigned char pre_c{0};
    std::stringstream ss;
};



} //namespace JudgeSever
