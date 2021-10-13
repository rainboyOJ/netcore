#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <string_view>

#include "base64.hpp"

//信息的传输格式

//message decode status
#define msg_dbg_out(one) std::cout << #one << ": " <<  one  << std::endl

//接收的信息
class message {
public:
    message() = default;
    message(int time,int memory,int uid ,std::string_view lang,std::string_view code)
        :time{time},memory{memory},uid{uid},lang{lang},code{code}
    {}

    int         time;   //时间限制 ms
    int         memory; //内存限制 mb
    int         uid;    //标识
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
        ss << "uid " << uid << '\n';
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
                    else if( key == "memory") memory = atoi(value.c_str());
                    status = MSGD_STATUS::new_line;
                }
                else value.push_back(c);
                return false;
        }
    }
};
