//全局配置类
//当你需要修改的时候就修改个文件
#pragma once

#include <type_traits>
#include <string_view>
using namespace std::literals;


constexpr std::size_t operator"" _MB ( unsigned long long const x ){
    return 1024L*1024L*x;
}

struct __config__{
    static constexpr int  session_expire = 15*24*60*70; // 15 days
    static constexpr auto static_dir = "www"sv;
    static constexpr auto upload_dir = "upload"sv;
    static constexpr int  alarm_time = 30; //seconds

    //服务器起动的配置
    static constexpr int port            = 8099;
    static constexpr int trigMode        = 0; // 暂时不能运行在ET模式,工作在LT模式
    static constexpr int timeoutMS       = 3*60*1000; //3分钟
    static constexpr bool OptLinger      = true; //优雅的关闭socket
    static constexpr int sqlPort         = 3306;
    static constexpr const char* sqlUser = "root";
    static constexpr const  char* sqlPwd = "root";
    static constexpr const char* dbName  = "rojcpp";
    static constexpr int connPoolNum     = 4;
    static constexpr int threadNum       = 4;
    static constexpr bool openLog        = true;
    static constexpr int logLevel        = 0;
    static constexpr int logQueSize      = 1;

    //其它配置
    static constexpr std::size_t maxRequestSize = 20_MB;
};


